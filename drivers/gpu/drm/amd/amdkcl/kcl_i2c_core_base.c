/*
 * i2c-core-base.c - a device driver for the i2c bus interface
 * Copyright (C) 1995-2000 Simon G. Vogl
 * Copyright (C) 2013-2019 Wolfram Sang <wsa@kernel.org>
 *
 * With some changes from Kyösti Mälkki <kmalkki@cc.hut.fi> and
 * Frodo Looijaard <frodol@dds.nl>
 */

#include <kcl/kcl_i2c.h>
#include <linux/device.h>

#ifndef HAVE_DEVM_I2C_ADD_ADAPTER
static void kcl_devm_i2c_del_adapter(void *adapter)
{
	i2c_del_adapter(adapter);
}

int kcl_devm_i2c_add_adapter(struct device *dev, struct i2c_adapter *adapter)
{
	int ret;

	ret = i2c_add_adapter(adapter);
	if (ret)
		return ret;

	return devm_add_action_or_reset(dev, kcl_devm_i2c_del_adapter, adapter);
}
EXPORT_SYMBOL_GPL(kcl_devm_i2c_add_adapter);
#endif
