/*
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <linux/list.h>
#include "amdgpu.h"
#include "amdgpu_aca.h"
#include "amdgpu_ras.h"

static struct aca_bank_error *new_bank_error(struct aca_error *aerr, struct aca_bank_info *info)
{
	struct aca_bank_error *bank_error;

	bank_error = kvzalloc_obj(*bank_error);
	if (!bank_error)
		return NULL;

	INIT_LIST_HEAD(&bank_error->node);
	memcpy(&bank_error->info, info, sizeof(*info));

	mutex_lock(&aerr->lock);
	list_add_tail(&bank_error->node, &aerr->list);
	aerr->nr_errors++;
	mutex_unlock(&aerr->lock);

	return bank_error;
}

static struct aca_bank_error *find_bank_error(struct aca_error *aerr, struct aca_bank_info *info)
{
	struct aca_bank_error *bank_error = NULL;
	struct aca_bank_info *tmp_info;
	bool found = false;

	mutex_lock(&aerr->lock);
	list_for_each_entry(bank_error, &aerr->list, node) {
		tmp_info = &bank_error->info;
		if (tmp_info->socket_id == info->socket_id &&
		    tmp_info->die_id == info->die_id) {
			found = true;
			goto out_unlock;
		}
	}

out_unlock:
	mutex_unlock(&aerr->lock);

	return found ? bank_error : NULL;
}

static void aca_bank_error_remove(struct aca_error *aerr, struct aca_bank_error *bank_error)
{
	if (!aerr || !bank_error)
		return;

	list_del(&bank_error->node);
	aerr->nr_errors--;

	kvfree(bank_error);
}

static struct aca_bank_error *get_bank_error(struct aca_error *aerr, struct aca_bank_info *info)
{
	struct aca_bank_error *bank_error;

	if (!aerr || !info)
		return NULL;

	bank_error = find_bank_error(aerr, info);
	if (bank_error)
		return bank_error;

	return new_bank_error(aerr, info);
}

int aca_error_cache_log_bank_error(struct aca_handle *handle, struct aca_bank_info *info,
				   enum aca_error_type type, u64 count)
{
	struct aca_error_cache *error_cache = &handle->error_cache;
	struct aca_bank_error *bank_error;
	struct aca_error *aerr;

	if (!handle || !info || type >= ACA_ERROR_TYPE_COUNT)
		return -EINVAL;

	if (!count)
		return 0;

	aerr = &error_cache->errors[type];
	bank_error = get_bank_error(aerr, info);
	if (!bank_error)
		return -ENOMEM;

	bank_error->count += count;

	return 0;
}

static void aca_error_init(struct aca_error *aerr, enum aca_error_type type)
{
	mutex_init(&aerr->lock);
	INIT_LIST_HEAD(&aerr->list);
	aerr->type = type;
	aerr->nr_errors = 0;
}

static void aca_init_error_cache(struct aca_handle *handle)
{
	struct aca_error_cache *error_cache = &handle->error_cache;
	int type;

	for (type = ACA_ERROR_TYPE_UE; type < ACA_ERROR_TYPE_COUNT; type++)
		aca_error_init(&error_cache->errors[type], type);
}

static void aca_error_fini(struct aca_error *aerr)
{
	struct aca_bank_error *bank_error, *tmp;

	mutex_lock(&aerr->lock);
	if (list_empty(&aerr->list))
		goto out_unlock;

	list_for_each_entry_safe(bank_error, tmp, &aerr->list, node)
		aca_bank_error_remove(aerr, bank_error);

out_unlock:
	mutex_unlock(&aerr->lock);
	mutex_destroy(&aerr->lock);
}

static void aca_fini_error_cache(struct aca_handle *handle)
{
	struct aca_error_cache *error_cache = &handle->error_cache;
	int type;

	for (type = ACA_ERROR_TYPE_UE; type < ACA_ERROR_TYPE_COUNT; type++)
		aca_error_fini(&error_cache->errors[type]);
}

static int add_aca_handle(struct amdgpu_device *adev, struct aca_handle_manager *mgr, struct aca_handle *handle,
			  const char *name, const struct aca_info *ras_info, void *data)
{
	memset(handle, 0, sizeof(*handle));

	handle->adev = adev;
	handle->mgr = mgr;
	handle->name = name;
	handle->hwip = ras_info->hwip;
	handle->mask = ras_info->mask;
	handle->bank_ops = ras_info->bank_ops;
	handle->data = data;
	aca_init_error_cache(handle);

	INIT_LIST_HEAD(&handle->node);
	list_add_tail(&handle->node, &mgr->list);
	mgr->nr_handles++;

	return 0;
}

static ssize_t aca_sysfs_read(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct aca_handle *handle = container_of(attr, struct aca_handle, aca_attr);

	/* NOTE: the aca cache will be auto cleared once read,
	 * So the driver should unify the query entry point, forward request to ras query interface directly */
	return amdgpu_ras_aca_sysfs_read(dev, attr, handle, buf, handle->data);
}

static int add_aca_sysfs(struct amdgpu_device *adev, struct aca_handle *handle)
{
	struct device_attribute *aca_attr = &handle->aca_attr;

	snprintf(handle->attr_name, sizeof(handle->attr_name) - 1, "aca_%s", handle->name);
	aca_attr->show = aca_sysfs_read;
	aca_attr->attr.name = handle->attr_name;
	aca_attr->attr.mode = S_IRUGO;
	sysfs_attr_init(&aca_attr->attr);

	return sysfs_add_file_to_group(&adev->dev->kobj,
				       &aca_attr->attr,
				       "ras");
}

int amdgpu_aca_add_handle(struct amdgpu_device *adev, struct aca_handle *handle,
			  const char *name, const struct aca_info *ras_info, void *data)
{
	struct amdgpu_aca *aca = &adev->aca;
	int ret;

	if (!amdgpu_aca_is_enabled(adev))
		return 0;

	ret = add_aca_handle(adev, &aca->mgr, handle, name, ras_info, data);
	if (ret)
		return ret;

	return add_aca_sysfs(adev, handle);
}

static void remove_aca_handle(struct aca_handle *handle)
{
	struct aca_handle_manager *mgr = handle->mgr;

	aca_fini_error_cache(handle);
	list_del(&handle->node);
	mgr->nr_handles--;
}

static void remove_aca_sysfs(struct aca_handle *handle)
{
	struct amdgpu_device *adev = handle->adev;
	struct device_attribute *aca_attr = &handle->aca_attr;

	if (adev->dev->kobj.sd)
		sysfs_remove_file_from_group(&adev->dev->kobj,
					     &aca_attr->attr,
					     "ras");
}

void amdgpu_aca_remove_handle(struct aca_handle *handle)
{
	if (!handle || list_empty(&handle->node))
		return;

	remove_aca_sysfs(handle);
	remove_aca_handle(handle);
}

static int aca_manager_init(struct aca_handle_manager *mgr)
{
	INIT_LIST_HEAD(&mgr->list);
	mgr->nr_handles = 0;

	return 0;
}

static void aca_manager_fini(struct aca_handle_manager *mgr)
{
	struct aca_handle *handle, *tmp;

	if (list_empty(&mgr->list))
		return;

	list_for_each_entry_safe(handle, tmp, &mgr->list, node)
		amdgpu_aca_remove_handle(handle);
}

bool amdgpu_aca_is_enabled(struct amdgpu_device *adev)
{
	return (adev->aca.is_enabled ||
		adev->debug_enable_ras_aca);
}

int amdgpu_aca_init(struct amdgpu_device *adev)
{
	struct amdgpu_aca *aca = &adev->aca;
	int ret;

	atomic_set(&aca->ue_update_flag, 0);

	ret = aca_manager_init(&aca->mgr);
	if (ret)
		return ret;

	return 0;
}

void amdgpu_aca_fini(struct amdgpu_device *adev)
{
	struct amdgpu_aca *aca = &adev->aca;

	aca_manager_fini(&aca->mgr);

	atomic_set(&aca->ue_update_flag, 0);
}

int amdgpu_aca_reset(struct amdgpu_device *adev)
{
	struct amdgpu_aca *aca = &adev->aca;

	atomic_set(&aca->ue_update_flag, 0);

	return 0;
}

void amdgpu_aca_set_smu_funcs(struct amdgpu_device *adev, const struct aca_smu_funcs *smu_funcs)
{
	struct amdgpu_aca *aca = &adev->aca;

	WARN_ON(aca->smu_funcs);
	aca->smu_funcs = smu_funcs;
}

int aca_bank_info_decode(struct aca_bank *bank, struct aca_bank_info *info)
{
	u64 ipid;
	u32 instidhi, instidlo;

	if (!bank || !info)
		return -EINVAL;

	ipid = bank->regs[ACA_REG_IDX_IPID];
	info->hwid = ACA_REG__IPID__HARDWAREID(ipid);
	info->mcatype = ACA_REG__IPID__MCATYPE(ipid);
	/*
	 * Unfied DieID Format: SAASS. A:AID, S:Socket.
	 * Unfied DieID[4:4] = InstanceId[0:0]
	 * Unfied DieID[0:3] = InstanceIdHi[0:3]
	 */
	instidhi = ACA_REG__IPID__INSTANCEIDHI(ipid);
	instidlo = ACA_REG__IPID__INSTANCEIDLO(ipid);
	info->die_id = ((instidhi >> 2) & 0x03);
	info->socket_id = ((instidlo & 0x1) << 2) | (instidhi & 0x03);

	return 0;
}

static int aca_bank_get_error_code(struct amdgpu_device *adev, struct aca_bank *bank)
{
	struct amdgpu_aca *aca = &adev->aca;
	const struct aca_smu_funcs *smu_funcs = aca->smu_funcs;

	if (!smu_funcs || !smu_funcs->parse_error_code)
		return -EOPNOTSUPP;

	return smu_funcs->parse_error_code(adev, bank);
}

int aca_bank_check_error_codes(struct amdgpu_device *adev, struct aca_bank *bank, int *err_codes, int size)
{
	int i, error_code;

	if (!bank || !err_codes)
		return -EINVAL;

	error_code = aca_bank_get_error_code(adev, bank);
	if (error_code < 0)
		return error_code;

	for (i = 0; i < size; i++) {
		if (err_codes[i] == error_code)
			return 0;
	}

	return -EINVAL;
}

int amdgpu_aca_smu_set_debug_mode(struct amdgpu_device *adev, bool en)
{
	struct amdgpu_aca *aca = &adev->aca;
	const struct aca_smu_funcs *smu_funcs = aca->smu_funcs;

	if (!smu_funcs || !smu_funcs->set_debug_mode)
		return -EOPNOTSUPP;

	return smu_funcs->set_debug_mode(adev, en);
}

#if defined(CONFIG_DEBUG_FS)
static int amdgpu_aca_smu_debug_mode_set(void *data, u64 val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;
	int ret;

	ret = amdgpu_ras_set_aca_debug_mode(adev, val ? true : false);
	if (ret)
		return ret;

	dev_info(adev->dev, "amdgpu set smu aca debug mode %s success\n", val ? "on" : "off");

	return 0;
}

static int aca_dump_show(struct seq_file *m, enum aca_smu_type type)
{
	return 0;
}

static int aca_dump_ce_show(struct seq_file *m, void *unused)
{
	return aca_dump_show(m, ACA_SMU_TYPE_CE);
}

static int aca_dump_ce_open(struct inode *inode, struct file *file)
{
	return single_open(file, aca_dump_ce_show, inode->i_private);
}

static const struct file_operations aca_ce_dump_debug_fops = {
	.owner = THIS_MODULE,
	.open = aca_dump_ce_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int aca_dump_ue_show(struct seq_file *m, void *unused)
{
	return aca_dump_show(m, ACA_SMU_TYPE_UE);
}

static int aca_dump_ue_open(struct inode *inode, struct file *file)
{
	return single_open(file, aca_dump_ue_show, inode->i_private);
}

static const struct file_operations aca_ue_dump_debug_fops = {
	.owner = THIS_MODULE,
	.open = aca_dump_ue_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

DEFINE_DEBUGFS_ATTRIBUTE(aca_debug_mode_fops, NULL, amdgpu_aca_smu_debug_mode_set, "%llu\n");
#endif

void amdgpu_aca_smu_debugfs_init(struct amdgpu_device *adev, struct dentry *root)
{
#if defined(CONFIG_DEBUG_FS)
	if (!root)
		return;

	debugfs_create_file("aca_debug_mode", 0200, root, adev, &aca_debug_mode_fops);
	debugfs_create_file("aca_ue_dump", 0400, root, adev, &aca_ue_dump_debug_fops);
	debugfs_create_file("aca_ce_dump", 0400, root, adev, &aca_ce_dump_debug_fops);
#endif
}
