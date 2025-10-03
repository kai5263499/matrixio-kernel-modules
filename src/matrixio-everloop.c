#include "matrixio-compat.h"
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "matrixio-core.h"

struct everloop_data {
	struct matrixio *mio;
	struct class *cl;
	dev_t devt;
	struct cdev cdev;
	struct device *device;
	int major;
};

ssize_t matrixio_everloop_write(struct file *pfile, const char __user *buffer,
				size_t length, loff_t *offset)
{
	int ret;
	void *kbuf;
	struct everloop_data *el = pfile->private_data;

	kbuf = kmalloc(length, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, buffer, length)) {
		kfree(kbuf);
		return -EFAULT;
	}

	ret = matrixio_write(el->mio, MATRIXIO_EVERLOOP_BASE, length, kbuf);
	kfree(kbuf);

	return ret < 0 ? ret : length;
}

int matrixio_everloop_open(struct inode *inode, struct file *filp)
{

	struct everloop_data *el;
	el = container_of(inode->i_cdev, struct everloop_data, cdev);

	filp->private_data = el; /* For use elsewhere */

	return 0;
}

struct file_operations matrixio_everloop_file_operations = {
    .owner = THIS_MODULE,
    .open = matrixio_everloop_open,
    .write = matrixio_everloop_write};

static int matrixio_everloop_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	(void)dev; /* unused parameter */
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}

static int matrixio_everloop_probe(struct platform_device *pdev)
{
	struct everloop_data *el;
	int ret;

	el = devm_kzalloc(&pdev->dev, sizeof(struct everloop_data), GFP_KERNEL);

	if (el == NULL)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, el);

	el->mio = dev_get_platdata(&pdev->dev);
	if (!el->mio) {
		dev_err(&pdev->dev, "Failed to get parent device data\n");
		return -EINVAL;
	}

	ret = alloc_chrdev_region(&el->devt, 0, 1, "matrixio_everloop");
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to allocate chrdev region\n");
		return ret;
	}

	el->cl = MATRIXIO_CLASS_CREATE("matrixio_everloop");
	if (IS_ERR(el->cl)) {
		ret = PTR_ERR(el->cl);
		dev_err(&pdev->dev, "Failed to create class: %d\n", ret);
		unregister_chrdev_region(el->devt, 1);
		return ret;
	}

	el->cl->dev_uevent = MATRIXIO_UEVENT_CAST(matrixio_everloop_uevent);

	el->device =
	    device_create(el->cl, NULL, el->devt, NULL, "matrixio_everloop");

	if (IS_ERR(el->device)) {
		ret = PTR_ERR(el->device);
		dev_err(&pdev->dev, "Unable to create device "
				    "for matrix; errno = %d\n", ret);
		class_destroy(el->cl);
		unregister_chrdev_region(el->devt, 1);
		return ret;
	}

	cdev_init(&el->cdev, &matrixio_everloop_file_operations);
	ret = cdev_add(&el->cdev, el->devt, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to add cdev\n");
		device_destroy(el->cl, el->devt);
		class_destroy(el->cl);
		unregister_chrdev_region(el->devt, 1);
		return ret;
	}

	return 0;
}

static MATRIXIO_REMOVE_RETURN_TYPE matrixio_everloop_remove(struct platform_device *pdev)
{
	struct everloop_data *el = dev_get_drvdata(&pdev->dev);

	cdev_del(&el->cdev);
	device_destroy(el->cl, el->devt);
	class_destroy(el->cl);
	unregister_chrdev_region(el->devt, 1);

	MATRIXIO_REMOVE_RETURN();
}

static struct platform_driver matrixio_everloop_driver = {
    .driver =
	{
	    .name = "matrixio-everloop",
	},
    .probe = matrixio_everloop_probe,
    .remove = matrixio_everloop_remove,
};

module_platform_driver(matrixio_everloop_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andres Calderon <andres.calderon@admobilize.com>");
MODULE_DESCRIPTION("MATRIXIO Everloop module");
