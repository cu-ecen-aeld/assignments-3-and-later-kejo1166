/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/mutex.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include "aesdchar.h"
#include "aesd-circular-buffer.h"

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Kenneth A. Jones");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev *pDev;

	PDEBUG("open");

	// Get pointer to structure
	pDev = container_of(inode->i_cdev, struct aesd_dev, cdev);

	// Save pointer in private_data
	filp->private_data = pDev;

	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release");
	//KJ\ Nothing to do here ?!
	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
						loff_t *f_pos)
{
	ssize_t retval = 0;
	ssize_t offset;
	ssize_t nRead;
	struct aesd_buffer_entry *pEntry = NULL;
	struct aesd_dev *pDev = (struct aesd_dev *)filp->private_data; // Get access to device driver

	PDEBUG("read %zu bytes with offset %lld", count, *f_pos);
	
	// Acquire device mutex
	if (mutex_lock_interruptible(&pDev->drv_mutex) != 0)
	{
		PDEBUG("Failed to acquire lock");
		return -ERESTARTSYS;
	}

	// Find the corresponding offset
	pEntry = aesd_circular_buffer_find_entry_offset_for_fpos(&pDev->cb,*f_pos, &offset );
	if (pEntry == NULL)
		goto done; // No matching entry not found

	// Calculate the number of bytes read
	nRead = pEntry->size - offset;
	if( count < nRead)
		nRead = count;

	// Copy data from kernel space to user space
	if(copy_to_user(buf, (pEntry->buffptr + offset), nRead) != 0 )
	{
		PDEBUG("Failed to copy %ld bytes from kernel space to user space", nRead);
		retval = -EFAULT;
		goto done;
	}

	// Update position
	*f_pos += nRead;

	// Return number of bytes read
	retval = nRead;

done:
	mutex_unlock(&pDev->drv_mutex);
	return retval; 
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
						 loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	struct aesd_dev *pDev = (struct aesd_dev *)filp->private_data; // Get access to device driver
	ssize_t nWrite;

	PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
	
	// Acquire device mutex
	retval = mutex_lock_interruptible(&pDev->drv_mutex);
	if (retval < 0)
	{
		PDEBUG("Failed to acquire lock");
		return retval;
	}

	// Check the current size of memory and allocate/reallocate as necessary
	if(pDev->entry.size == 0)
	{
		// Allocate memory
		pDev->entry.buffptr = kzalloc((sizeof(char) * count), GFP_KERNEL);

		// Check for allocation fail
		if (pDev->entry.buffptr == 0)
		{
			// Failled to allocate memory
			retval = -ENOMEM;
			goto done;
		}
	}
	else
	{
		// Re-allocate memory
		pDev->entry.buffptr = krealloc(pDev->entry.buffptr, (pDev->entry.size + count), GFP_KERNEL);

		// Check for re-allocation fail
		if (pDev->entry.buffptr == 0)
		{
			goto done;
		}
	}

	// Copy from user space to kernel space
	nWrite = copy_from_user((void *)(&pDev->entry.buffptr[pDev->entry.size]), buf, count);

	// Calculate the number of bytes of byte remaining to write
	retval = count - nWrite;

	// Update entry size
	pDev->entry.size += retval;

	// Check for termination
	if (memchr(pDev->entry.buffptr, '\n', pDev->entry.size) != NULL)
	{
		// Add new entry
		aesd_circular_buffer_add_entry(&pDev->cb, &pDev->entry);

		// Reset size and buffer pointer
		pDev->entry.size = 0;
		pDev->entry.buffptr = 0;
	}

	// Reset position
	*f_pos = 0;

done:
	mutex_unlock(&pDev->drv_mutex);
	return retval;
}
struct file_operations aesd_fops = {
	 .owner = THIS_MODULE,
	 .read = aesd_read,
	 .write = aesd_write,
	 .open = aesd_open,
	 .release = aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
	{
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}

int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1,
										  "aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0)
	{
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device, 0, sizeof(struct aesd_dev));

	// Initialize the mutex
	mutex_init(&aesd_device.drv_mutex);

	// Initialize the circular buffer
	aesd_circular_buffer_init(&aesd_device.cb);

	result = aesd_setup_cdev(&aesd_device);

	if (result)
	{
		unregister_chrdev_region(dev, 1);
	}
	return result;
}

void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	// Free all allocated memory in the circular buffer
	aesd_circular_buffer_deinit(&aesd_device.cb);
	
	unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
