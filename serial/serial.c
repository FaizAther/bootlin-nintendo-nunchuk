// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/serial_reg.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>

#define OMAP_UART_SCR_DMAMODE_CTL3 0x7
#define OMAP_UART_SCR_TX_TRIG_GRANU1 BIT(6)


#define SERIAL_RESET_COUNTER 0
#define SERIAL_GET_COUNTER  1

#define SERIAL_BUFSIZE 16



struct serial_dev {
        void __iomem *regs;
        unsigned int counter;
        struct miscdevice miscdev;
        char rx_buf[SERIAL_BUFSIZE];
        unsigned int buf_rd;
        unsigned int buf_wr;
        spinlock_t serial_lock;
        char tx_buf[SERIAL_BUFSIZE];
        unsigned int buf_tx_rd;
        unsigned int buf_tx_wr;
        wait_queue_head_t wait;
        struct dma_chan *txchan;
        dma_addr_t dma_map_resource;
        dma_addr_t fifo_dma_addr;
        struct device *dev;
};


// struct serial_dev *serial;


static u32 reg_read(struct serial_dev * serial, unsigned int reg)
{
        return readl(serial->regs + reg * 4);;
}

static void reg_write(struct serial_dev *serial, u32 val, unsigned int reg)
{
        writel(val, serial->regs + reg * 4);
}

static void serial_putchar(struct serial_dev *serial, char c)
{
serial_redo:
        while (!(reg_read(serial, UART_LSR) & UART_LSR_THRE)) {
                cpu_relax();
        }
        pr_info("LSR after wait: 0x%08x\n", reg_read(serial, UART_LSR));
        // pr_info("sending char: %x\n", c);
        if (c == '\r') {
                reg_write(serial, '\r', UART_TX);
                // pr_info("saw enter\n");
                c = '\n';
                goto serial_redo;
        }
        reg_write(serial, c, UART_TX);
}

static long serial_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        struct serial_dev *serial = file->private_data;

        switch (cmd) {
        case SERIAL_RESET_COUNTER:
                serial->counter = 0;
                pr_info("Counter reset\n");
                return 0;
        case SERIAL_GET_COUNTER:
                if (copy_to_user((unsigned int __user *)arg, &serial->counter,
                                 sizeof(serial->counter)))
                        return -EFAULT;
                return 0;
        default:
                return -ENOTTY;
        }
}

// Write file op allows user space to send characters via echo/write
static ssize_t serial_write_pio(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
        struct serial_dev *serial = file->private_data;
        char kbuf[64]; // Temporary safe kernel buffer
        size_t bytes_to_copy;
        size_t i;

        if (count == 0)
                return 0;

        // Limit the transfer block size to prevent kernel stack overflow
        bytes_to_copy = min(count, sizeof(kbuf));

        // Copy the entire block cleanly from user-space in one operation
        if (copy_from_user(kbuf, user_buffer, bytes_to_copy)) {
                return -EFAULT;
        }

        // Safely loop through our private kernel memory space
        for (i = 0; i < bytes_to_copy; i++) {
                serial_putchar(serial, kbuf[i]);
        }

        return bytes_to_copy;
}

static int serial_open(struct inode *inode, struct file *file)
{
        // Safely extract your serial_dev structure pointer container from container_of
        struct miscdevice *mdev = file->private_data;
        struct serial_dev *serial = container_of(mdev, struct serial_dev, miscdev);
        
        file->private_data = serial;
        return 0;
}

static ssize_t serial_read_pio(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
        struct serial_dev *serial = file->private_data;
        size_t bytes_read = 0;

        if (count == 0) {
                return 0;
        }
        
        wait_event_interruptible(serial->wait, serial->buf_rd != serial->buf_wr);

        if (serial->buf_rd == serial->buf_wr) {
                return 0;
        }

        // Check if there is data available in the hardware Rx FIFO
        // UART_LSR_DR indicates 'Data Ready'
        while (serial->buf_rd != serial->buf_wr && bytes_read < count) {
                if (copy_to_user(user_buffer+bytes_read, serial->rx_buf + serial->buf_rd, 1))
                        return -EFAULT;
                serial->buf_rd = (serial->buf_rd + 1) % SERIAL_BUFSIZE;
                ++bytes_read;
        }

        // Return 0 if no data is available (non-blocking behaviour)
        return bytes_read; 
}

static int serial_close(struct inode *inode, struct file *file)
{
        return 0;
}

static __poll_t serial_poll(struct file *file, struct poll_table_struct *wait)
{
        struct serial_dev *serial = file->private_data;
        __poll_t mask = 0;

        // Register the wait queue with the poll table
        poll_wait(file, &serial->wait, wait);

        // Check if there is data available to be read
        if (serial->buf_rd != serial->buf_wr)
                mask |= EPOLLIN | EPOLLRDNORM;

        // You can also signal write readiness if your TX FIFO isn't full
        mask |= EPOLLOUT | EPOLLWRNORM; 

        return mask;
}

static ssize_t serial_write_dma(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
        return 0;
}

void serial_cleanup_dma(struct platform_device *pdev, struct serial_dev *serial)
{
        int err = dmaengine_terminate_sync(serial->txchan);
        if (err) {
                dev_err(serial->dev, "Failed to terminate DMA: %d\n", err);
        }
        dma_unmap_resource(serial->dev, serial->fifo_dma_addr, 4, DMA_TO_DEVICE, 0);
        dma_release_channel(serial->txchan);
        return;
}

static struct file_operations serial_fops_pio = {
    .owner   = THIS_MODULE,
    .open    = serial_open,
    .read    = serial_read_pio,
    .write   = serial_write_pio,
    .unlocked_ioctl   = serial_ioctl,
    .poll    = serial_poll,
    .release = serial_close,
};

static struct file_operations serial_fops_dma = {
    .owner   = THIS_MODULE,
    .open    = serial_open,
    .read    = serial_read_pio,
    .write   = serial_write_dma,
    .unlocked_ioctl   = serial_ioctl,
    .poll    = serial_poll,
    .release = serial_close,
};

const struct of_device_id bootlin_of_match[] = {
        { .compatible = "bootlin,serial", },
        { }
};
MODULE_DEVICE_TABLE(of, bootlin_of_match);

/**
struct platform_device {
        const char        *name;
        u32                id;
        struct device      dev;
        u32                num_resources;
        struct resource    *resource;
};
*/

static irqreturn_t serial_irq_handler(int irq, void *dev_id)
{
        pr_info("hello irq{%d}\n", irq);
        struct serial_dev *serial = (struct serial_dev *)dev_id;
        char c;
        unsigned long flags;
        while (reg_read(serial, UART_LSR) & UART_LSR_DR) {
                c = reg_read(serial, UART_RX); // Read byte from Rx register
                pr_info("byte read %c\n", c);
                spin_lock_irqsave(&serial->serial_lock, flags);
                serial->rx_buf[serial->buf_wr] = c;
                serial->buf_wr = (serial->buf_wr + 1) % SERIAL_BUFSIZE;
                spin_unlock_irqrestore(&serial->serial_lock, flags);
        }
        wake_up(&serial->wait);
        return IRQ_HANDLED;
}

int serial_init_dma(struct platform_device *pdev, struct serial_dev *serial)
{

        serial->miscdev.fops = &serial_fops_dma;
        struct dma_slave_config txconf = {};
        struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!res) {
                return -ENOMEM;
        }
        serial->fifo_dma_addr = dma_map_resource(serial->dev, res->start + UART_TX * 4, 4, DMA_TO_DEVICE, 0);
        if (dma_mapping_error(serial->dev, serial->fifo_dma_addr)) {
                dev_err(serial->dev, "Failed to map resource: %pad\n", &serial->fifo_dma_addr);
                return -ENOMEM;
        }
        txconf.direction = DMA_MEM_TO_DEV;
        txconf.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
        txconf.dst_addr = serial->fifo_dma_addr;
        int ret = dmaengine_slave_config(serial->txchan, &txconf);
        if (ret) {
                dev_err(serial->dev, "Failed to configure DMA: %d\n", ret);
                return ret;
        }
        reg_write(serial, OMAP_UART_SCR_DMAMODE_CTL3 | OMAP_UART_SCR_TX_TRIG_GRANU1, UART_OMAP_SCR);
        return 0;
}

static int serial_probe(struct platform_device *pdev)
{
        struct serial_dev *serial;
        unsigned int uartclk;
        unsigned int baud_divisor;
        int ret;
        int irq_num;

        pr_info("Called %s\n", __func__);
        pr_info("platform_device: %p\n", pdev);
        pr_info("platform_device->name: %s\n", pdev->name);
        pr_info("platform_device->id: %d\n", pdev->id);
        pr_info("platform_device->dev: %p\n", &pdev->dev);
        pr_info("platform_device->dev.driver_data: %p\n", &pdev->dev.driver_data);
        pr_info("platform_device->dev.driver_data: %p\n", pdev->dev.driver_data);
        serial = devm_kzalloc(&pdev->dev, sizeof(struct serial_dev), GFP_KERNEL);
        serial->dev = &pdev->dev;
        if (!serial) {
                pr_err("Failed to allocate memory for serial_dev\n");
                return -ENOMEM;
        }
        serial->regs = devm_platform_ioremap_resource(pdev, 0);
        if (IS_ERR(serial->regs)) {
                return PTR_ERR(serial->regs);
        }
        pr_info("serial: %p\n", pdev->dev.driver_data);
        pr_info("platform_device->num_resources: %d\n", pdev->num_resources);
        pr_info("platform_device->resource: %p\n", pdev->resource);
        platform_set_drvdata(pdev, serial);
        pm_runtime_enable(&pdev->dev);
        pm_runtime_get_sync(&pdev->dev);
        ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency", &uartclk);
        if (ret) {
                dev_err(&pdev->dev, "clock-frequency property not found in Device Tree\n");
                return ret;
        }
        baud_divisor = uartclk / 16 / 115200;
        reg_write(serial, 0x07, UART_OMAP_MDR1);
        reg_write(serial, 0x00, UART_LCR);
        reg_write(serial, UART_LCR_DLAB, UART_LCR);
        reg_write(serial, baud_divisor & 0xff, UART_DLL);
        reg_write(serial, (baud_divisor >> 8) & 0xff, UART_DLM);
        reg_write(serial, UART_LCR_WLEN8, UART_LCR);
        reg_write(serial, 0x00, UART_OMAP_MDR1);
        reg_write(serial, UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT, UART_FCR);
        reg_write(serial, UART_IER_RDI, UART_IER);

        pr_info("uartclk from DT: %u\n", uartclk);
        pr_info("initial LSR: 0x%08x\n", reg_read(serial, UART_LSR));
        serial_putchar(serial, 'H');

        // Dynamically configure and register the misc device instance
        serial->miscdev.minor = MISC_DYNAMIC_MINOR;
        serial->miscdev.name = devm_kasprintf(&pdev->dev, GFP_KERNEL, 
                                              "bootlin_uart_%s", 
                                              pdev->name);
        if (!serial->miscdev.name) {
                dev_err(&pdev->dev, "Failed to allocate unique name string\n");
                pm_runtime_put_sync(&pdev->dev);
                pm_runtime_disable(&pdev->dev);
                return -ENOMEM;
        }

        serial->miscdev.parent = &pdev->dev;  // Tie node lifecycle to device tree entry

        ret = misc_register(&serial->miscdev);
        if (ret) {
                dev_err(&pdev->dev, "Failed to register misc device\n");
                pm_runtime_put_sync(&pdev->dev);
                pm_runtime_disable(&pdev->dev);
                return ret;
        }

        irq_num = platform_get_irq(pdev, 0);
        if (irq_num < 0) {
                dev_err(&pdev->dev, "Failed to get IRQ\n");
                misc_deregister(&serial->miscdev);
                pm_runtime_put_sync(&pdev->dev);
                pm_runtime_disable(&pdev->dev);
                return irq_num;
        }

        ret = devm_request_irq(&pdev->dev, irq_num, serial_irq_handler, 0, "serial_irq", serial);
        if (ret) {
                dev_err(&pdev->dev, "Failed to request IRQ\n");
                misc_deregister(&serial->miscdev);
                pm_runtime_put_sync(&pdev->dev);
                pm_runtime_disable(&pdev->dev);
                return ret;
        }

        init_waitqueue_head(&serial->wait);
        serial->buf_rd = serial->buf_wr = 0;

        spin_lock_init(&serial->serial_lock);

        serial->txchan = dma_request_chan(&pdev->dev, pdev->name);
        if (IS_ERR(serial->txchan)) {
                ret = PTR_ERR(serial->txchan);
                dev_err(&pdev->dev, "Failed to request TX channel: %pe\n", serial->txchan);
                serial->miscdev.fops = &serial_fops_pio;
        } else {
                serial_init_dma(pdev, serial);
        }

        dev_info(&pdev->dev, "Misc device registered successfully\n");
        return 0;
}

static int serial_remove(struct platform_device *pdev)
{
        pr_info("Called %s\n", __func__);
        // serial_putchar(serial, '}');
        struct serial_dev *serial = platform_get_drvdata(pdev);

        // Clean up user space exposed interfaces before power-down
        misc_deregister(&serial->miscdev);

        pm_runtime_put_sync(&pdev->dev);
        pm_runtime_disable(&pdev->dev);
        return 0;
}

static struct platform_driver serial_driver = {
        .driver = {
                .name = "serial",
                .of_match_table = bootlin_of_match,
                .owner = THIS_MODULE,
        },
        .probe = serial_probe,
        .remove = serial_remove,
};
module_platform_driver(serial_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Bootlin Serial");
MODULE_AUTHOR("Faiz Ather");
