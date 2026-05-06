// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Advanced Micro Devices, Inc.
 */

#include <linux/pm_runtime.h>
#include <linux/vmalloc.h>
#include <media/v4l2-ioctl.h>

#include "isp4.h"

#define ISP4_DRV_NAME "amd_isp_capture"

static const struct {
	const char *name;
	u32 status_mask;
	u32 en_mask;
	u32 ack_mask;
	u32 rb_int_num;
} isp4_irq[] = {
	/* The IRQ order is aligned with the isp4_subdev.fw_resp_thread order */
	{
		.name = "isp_irq_global",
		.rb_int_num = 4, /* ISP_4_1__SRCID__ISP_RINGBUFFER_WPT12 */
	},
	{
		.name = "isp_irq_stream1",
		.rb_int_num = 0, /* ISP_4_1__SRCID__ISP_RINGBUFFER_WPT9 */
	},
};

static irqreturn_t isp4_irq_handler(int irq, void *arg)
{
	return IRQ_HANDLED;
}

static int isp4_capture_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int irq[ARRAY_SIZE(isp4_irq)];
	struct isp4_device *isp_dev;
	int ret;

	isp_dev = devm_kzalloc(dev, sizeof(*isp_dev), GFP_KERNEL);
	if (!isp_dev)
		return -ENOMEM;

	dev->init_name = ISP4_DRV_NAME;

	for (size_t i = 0; i < ARRAY_SIZE(isp4_irq); i++) {
		irq[i] = platform_get_irq(pdev, isp4_irq[i].rb_int_num);
		if (irq[i] < 0)
			return dev_err_probe(dev, irq[i],
					     "fail to get irq %d\n",
					     isp4_irq[i].rb_int_num);

		ret = devm_request_irq(dev, irq[i], isp4_irq_handler,
				       IRQF_NO_AUTOEN, isp4_irq[i].name, dev);
		if (ret)
			return dev_err_probe(dev, ret, "fail to req irq %d\n",
					     irq[i]);
	}

	isp_dev->v4l2_dev.mdev = &isp_dev->mdev;

	strscpy(isp_dev->mdev.model, "amd_isp41_mdev",
		sizeof(isp_dev->mdev.model));
	isp_dev->mdev.dev = dev;
	media_device_init(&isp_dev->mdev);

	snprintf(isp_dev->v4l2_dev.name, sizeof(isp_dev->v4l2_dev.name),
		 "AMD-V4L2-ROOT");
	ret = v4l2_device_register(dev, &isp_dev->v4l2_dev);
	if (ret) {
		dev_err_probe(dev, ret, "fail register v4l2 device\n");
		goto err_clean_media;
	}

	pm_runtime_set_suspended(dev);
	pm_runtime_enable(dev);
	ret = media_device_register(&isp_dev->mdev);
	if (ret) {
		dev_err_probe(dev, ret, "fail to register media device\n");
		goto err_isp4_deinit;
	}

	platform_set_drvdata(pdev, isp_dev);

	return 0;

err_isp4_deinit:
	pm_runtime_disable(dev);
	v4l2_device_unregister(&isp_dev->v4l2_dev);
err_clean_media:
	media_device_cleanup(&isp_dev->mdev);

	return ret;
}

static void isp4_capture_remove(struct platform_device *pdev)
{
	struct isp4_device *isp_dev = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	media_device_unregister(&isp_dev->mdev);
	pm_runtime_disable(dev);
	v4l2_device_unregister(&isp_dev->v4l2_dev);
	media_device_cleanup(&isp_dev->mdev);
}

static struct platform_driver isp4_capture_drv = {
	.probe = isp4_capture_probe,
	.remove = isp4_capture_remove,
	.driver = {
		.name = ISP4_DRV_NAME,
	}
};

module_platform_driver(isp4_capture_drv);

MODULE_ALIAS("platform:" ISP4_DRV_NAME);
MODULE_IMPORT_NS("DMA_BUF");

MODULE_DESCRIPTION("AMD ISP4 Driver");
MODULE_AUTHOR("Bin Du <bin.du@amd.com>");
MODULE_AUTHOR("Pratap Nirujogi <pratap.nirujogi@amd.com>");
MODULE_LICENSE("GPL");
