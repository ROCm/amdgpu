/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2025 Advanced Micro Devices, Inc.
 */

#ifndef _ISP4_H_
#define _ISP4_H_

#include <media/v4l2-device.h>
#include <media/videobuf2-memops.h>

struct isp4_device {
	struct v4l2_device v4l2_dev;
	struct media_device mdev;
};

#endif /* _ISP4_H_ */
