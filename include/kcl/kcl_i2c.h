/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * i2c.h - definitions for the Linux i2c bus interface
 * Copyright (C) 1995-2000 Simon G. Vogl
 * Copyright (C) 2013-2019 Wolfram Sang <wsa@kernel.org>
 *
 * With some changes from Kyösti Mälkki <kmalkki@cc.hut.fi> and
 * Frodo Looijaard <frodol@dds.nl>
 */
#ifndef _KCL_KCL_I2C_H
#define _KCL_KCL_I2C_H

#include <linux/i2c.h>
#include <linux/printk.h>

#ifdef HAVE_I2C_NEW_CLIENT_DEVICE
extern struct i2c_client *
i2c_new_client_device(struct i2c_adapter *adap, struct i2c_board_info const *info);
#else
static inline struct i2c_client *
i2c_new_client_device(struct i2c_adapter *adap, struct i2c_board_info const *info)
{
	return i2c_new_device(adap, info);
}
#endif

#ifndef I2C_AQ_NO_ZERO_LEN
#define I2C_AQ_NO_ZERO_LEN_READ         BIT(5)
#define I2C_AQ_NO_ZERO_LEN_WRITE        BIT(6)
#define I2C_AQ_NO_ZERO_LEN              (I2C_AQ_NO_ZERO_LEN_READ | I2C_AQ_NO_ZERO_LEN_WRITE)
#endif

#ifndef HAVE_DEVM_I2C_ADD_ADAPTER
extern int kcl_devm_i2c_add_adapter(struct device *dev, struct i2c_adapter *adapter);
#define devm_i2c_add_adapter kcl_devm_i2c_add_adapter
#endif

#endif
