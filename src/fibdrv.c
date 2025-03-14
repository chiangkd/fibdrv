#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

#ifndef FIBMODE
#define FIBMODE 1
#endif

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

// static DEFINE_KTIME(fib_kt);

static ktime_t fib_kt;

long long (*fib_method)(
    long long);  // function pointer point to the selected method

static long long fib_sequence_fd_iter(long long k)
{
    // Fibonacci sequence: 0, 1, 1, 2, 3, 5, 8, 13, ...
    if (k <= 2)
        return !!k;

    long long fk0 = 1;  // F(k), F(2k)
    long long fk1 = 1;  // F(k+1), F(2k+1)

    uint8_t count = 63 - __builtin_clzll(k);

    for (uint64_t i = count, f2k0, f2k1; i-- > 0;) {
        /*  F(2k) = F(k)[2F(k+1) - F(k)]
         *  F(2k+1) = F(k+1)^2 + F(k)^2
         */
        f2k0 = fk0 * ((fk1 << 1) - fk0);
        f2k1 = fk1 * fk1 + fk0 * fk0;

        if (k & (1UL << i)) {
            fk0 = f2k1;
            fk1 = f2k0 + f2k1;
        } else {
            fk0 = f2k0;
            fk1 = f2k1;
        }
    }
    return fk0;
}

static long long fib_sequence_fd_recur(long long k)
{
    if (k <= 2)
        return !!k;
    /*  F(2k) = F(k)[2F(k+1) - F(k)]    - (1)
     *  F(2k+1) = F(k+1)^2 + F(k)^2     - (2)
     */
    long long n = fib_sequence_fd_recur(k >> 1);
    long long n1 = fib_sequence_fd_recur((k >> 1) + 1);

    // return 2n or 2n+1
    if (k & 1)
        return n * n + n1 * n1;  // - (2)
    return n * ((n1 << 1) - n);  // - (1)
}

static long long fib_sequence(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}


void mode_select(void)
{
    switch (FIBMODE) {
    case 1:
        fib_method = &fib_sequence;
        break;
    case 2:
        fib_method = &fib_sequence_fd_iter;
        break;
    case 3:
        fib_method = &fib_sequence_fd_recur;
        break;

    default:
        break;
    }
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    // return (ssize_t) fib_sequence(*offset);
    mode_select();
    fib_kt = ktime_get();
    /* multiple method under test */
    ssize_t ret = fib_method(*offset);
    fib_kt = ktime_sub(ktime_get(), fib_kt);
    return ret;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    // return 1;
    return ktime_to_ns(fib_kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
