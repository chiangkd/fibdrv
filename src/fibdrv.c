#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>


#include "../inc/bignum.h"
MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 1000

#ifndef FIBMODE
#define FIBMODE 1
#endif

typedef struct _hdata_node {
    bn *data;
    struct hlist_node list;
} hdata_node;

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static struct hlist_head htable[MAX_LENGTH + 1];  // hash table (share memory)
static hdata_node *dnode = NULL;                  // data node


static ktime_t fib_kt;

static void (*fib_method)(
    bn *,
    unsigned int);  // function pointer point to the selected method

static int fib_open(struct inode *inode, struct file *file)
{
    // if (!mutex_trylock(&fib_mutex)) {
    //     printk(KERN_ALERT "fibdrv is in use");
    //     return -EBUSY;
    // }
    // mutex_lock(&fib_mutex);
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    // mutex_unlock(&fib_mutex);
    return 0;
}


static void mode_select(void)
{
    switch (FIBMODE) {
    case 1:
        fib_method = &bn_fib;
        break;
    case 2:
        fib_method = &bn_fib_fdoubling;
        break;
    case 3:
        fib_method = &bn_fib_fdoubling_v1;
        break;
    case 4:
        fib_method = &bn_fib_fdoubling_v2;
        break;
    default:
        break;
    }
}

static int is_in_ht(loff_t *offset)
{
    int key = (int) *(offset);
    if (hlist_empty(&htable[key])) {
        printk(KERN_INFO "No find offset = %d in thread %d\n", key,
               current->pid);
        return 0; /* no in hash table */
    }
    return 1;
}

static void release_memory(void)
{
    struct hlist_node *n = NULL;
    /* go through and free hashtable */
    for (int i = 0; i < MAX_LENGTH; i++) {
        hlist_for_each_entry_safe(dnode, n, &htable[i], list)
        {
            bn_free(dnode->data);
            hlist_del(&dnode->list);
            kfree(dnode);
        }
    }
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    char *p = NULL;
    size_t len = 0, left = 0;
    bn *fib = NULL;
    int key = (int) *offset;

    /* critical section */
    mutex_lock(&fib_mutex);
    if (is_in_ht(offset)) {
        printk(KERN_INFO "find offset = %d in thread %d\n", key, current->pid);
        fib = hlist_entry(htable[key].first, hdata_node, list)->data;
    } else {
        fib = bn_alloc(1);
        dnode = kcalloc(1, sizeof(hdata_node), GFP_KERNEL);
        if (dnode == NULL)
            printk("kcalloc failed \n");
        mode_select();
        fib_method(fib, *offset);
        dnode->data = fib;
        INIT_HLIST_NODE(&dnode->list);
        hlist_add_head(&dnode->list, &htable[key]);  // add to hash table
    }
    mutex_unlock(&fib_mutex);

    p = bn_to_string(fib);
    len = strlen(p) + 1;
    left = copy_to_user(buf, p, len);

    kfree(p);
    return left;
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

    for (int i = 0; i <= MAX_LENGTH; i++) {
        INIT_HLIST_HEAD(&htable[i]);
    }

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
    release_memory();
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
