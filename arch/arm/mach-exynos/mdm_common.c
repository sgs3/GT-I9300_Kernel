/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/debugfs.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#ifndef CONFIG_ARCH_EXYNOS
#include <linux/mfd/pmic8058.h>
#endif
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <mach/mdm2.h>
#include <mach/restart.h>
#include <mach/subsystem_notif.h>
#include <mach/subsystem_restart.h>
#include <linux/msm_charm.h>
#ifndef CONFIG_ARCH_EXYNOS
#include "msm_watchdog.h"
#endif
#include "mdm_private.h"

#ifdef CONFIG_ARCH_EXYNOS
#include <linux/interrupt.h>
#include <plat/gpio-cfg.h>
#endif

#define MDM_MODEM_TIMEOUT	6000
#define MDM_MODEM_DELTA	100
#define MDM_BOOT_TIMEOUT	60000L
#define MDM_RDUMP_TIMEOUT	60000L

static int mdm_debug_on;
static struct workqueue_struct *mdm_queue;

#define EXTERNAL_MODEM "external_modem"

static struct mdm_modem_drv *mdm_drv;

DECLARE_COMPLETION(mdm_needs_reload);
DECLARE_COMPLETION(mdm_boot);
DECLARE_COMPLETION(mdm_ram_dumps);

static int first_boot = 1;

long mdm_modem_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	int status, ret = 0;

	if (_IOC_TYPE(cmd) != CHARM_CODE) {
		pr_err("%s: invalid ioctl code\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: Entering ioctl cmd = %d\n", __func__, _IOC_NR(cmd));
	switch (cmd) {
	case WAKE_CHARM:
		pr_info("%s: Powering on mdm\n", __func__);
		mdm_drv->ops->power_on_mdm_cb(mdm_drv);
		break;
	case CHECK_FOR_BOOT:
		if (gpio_get_value(mdm_drv->mdm2ap_status_gpio) == 0)
			put_user(1, (unsigned long __user *) arg);
		else
			put_user(0, (unsigned long __user *) arg);
		break;
	case NORMAL_BOOT_DONE:
		pr_info("%s: check if mdm is booted up\n", __func__);
		get_user(status, (unsigned long __user *) arg);
		if (status) {
			pr_debug("%s: normal boot failed\n", __func__);
			mdm_drv->mdm_boot_status = -EIO;
		} else {
			pr_info("%s: normal boot done\n", __func__);
			mdm_drv->mdm_boot_status = 0;
		}
		mdm_drv->mdm_ready = 1;

		if (mdm_drv->ops->normal_boot_done_cb != NULL)
			mdm_drv->ops->normal_boot_done_cb(mdm_drv);

		if (!first_boot)
			complete(&mdm_boot);
		else
			first_boot = 0;
		break;
	case RAM_DUMP_DONE:
		pr_info("%s: mdm done collecting RAM dumps\n", __func__);
		get_user(status, (unsigned long __user *) arg);
		if (status)
			mdm_drv->mdm_ram_dump_status = -EIO;
		else {
			pr_info("%s: ramdump collection completed\n", __func__);
			mdm_drv->mdm_ram_dump_status = 0;
		}
		complete(&mdm_ram_dumps);
		break;
	case WAIT_FOR_RESTART:
		pr_info("%s: wait for mdm to need images reloaded\n",
				__func__);
		ret = wait_for_completion_interruptible(&mdm_needs_reload);
		if (!ret)
			put_user(mdm_drv->boot_type,
					 (unsigned long __user *) arg);
		INIT_COMPLETION(mdm_needs_reload);
		break;
	default:
		pr_err("%s: invalid ioctl cmd = %d\n", __func__, _IOC_NR(cmd));
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void mdm_fatal_fn(struct work_struct *work)
{
	pr_info("%s: Reseting the mdm due to an errfatal\n", __func__);
	subsystem_restart(EXTERNAL_MODEM);
}

static DECLARE_WORK(mdm_fatal_work, mdm_fatal_fn);

static void mdm_status_fn(struct work_struct *work)
{
	int value = gpio_get_value(mdm_drv->mdm2ap_status_gpio);

	if (!mdm_drv->mdm_ready)
		return;

	mdm_drv->ops->status_cb(mdm_drv, value);

	pr_err("%s: status:%d\n", __func__, value);

	if ((value == 0)) {
		pr_info("%s: unexpected reset external modem\n", __func__);
		subsystem_restart(EXTERNAL_MODEM);
	} else if (value == 1) {
		pr_info("%s: status = 1: mdm is now ready\n", __func__);
	}
}

static DECLARE_WORK(mdm_status_work, mdm_status_fn);

static void mdm_disable_irqs(void)
{
	disable_irq_nosync(mdm_drv->mdm_errfatal_irq);
	disable_irq_nosync(mdm_drv->mdm_status_irq);

}

static irqreturn_t mdm_errfatal(int irq, void *dev_id)
{
	pr_debug("%s: mdm got errfatal interrupt\n", __func__);
	if (mdm_drv->mdm_ready &&
		(gpio_get_value(mdm_drv->mdm2ap_status_gpio) == 1)) {
		pr_debug("%s: scheduling work now\n", __func__);
		queue_work(mdm_queue, &mdm_fatal_work);
	}
	return IRQ_HANDLED;
}

static int mdm_modem_open(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations mdm_modem_fops = {
	.owner		= THIS_MODULE,
	.open		= mdm_modem_open,
	.unlocked_ioctl	= mdm_modem_ioctl,
};


static struct miscdevice mdm_modem_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "mdm",
	.fops	= &mdm_modem_fops
};

static int mdm_panic_prep(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	int i;

	pr_debug("%s: setting AP2MDM_ERRFATAL high for a non graceful reset\n",
			 __func__);
	mdm_disable_irqs();
	gpio_set_value(mdm_drv->ap2mdm_errfatal_gpio, 1);

	for (i = MDM_MODEM_TIMEOUT; i > 0; i -= MDM_MODEM_DELTA) {
		/* pet_watchdog(); */
		mdelay(MDM_MODEM_DELTA);
		if (gpio_get_value(mdm_drv->mdm2ap_status_gpio) == 0)
			break;
	}
	if (i <= 0)
		pr_err("%s: MDM2AP_STATUS never went low\n", __func__);
	return NOTIFY_DONE;
}

static struct notifier_block mdm_panic_blk = {
	.notifier_call  = mdm_panic_prep,
};

static irqreturn_t mdm_status_change(int irq, void *dev_id)
{
	int value = gpio_get_value(mdm_drv->mdm2ap_status_gpio);
	pr_err("%s: mdm sent status change interrupt : %d\n", __func__, value);

	queue_work(mdm_queue, &mdm_status_work);

	return IRQ_HANDLED;
}

static int mdm_subsys_shutdown(const struct subsys_data *crashed_subsys)
{
	pr_info("%s\n", __func__);
	mdm_drv->mdm_ready = 0;
	gpio_direction_output(mdm_drv->ap2mdm_errfatal_gpio, 1);
	if (mdm_drv->pdata->ramdump_delay_ms > 0) {
		/* Wait for the external modem to complete
		 * its preparation for ramdumps.
		 */
		msleep(mdm_drv->pdata->ramdump_delay_ms);
	}
	mdm_drv->ops->power_down_mdm_cb(mdm_drv);
	return 0;
}

static int mdm_subsys_powerup(const struct subsys_data *crashed_subsys)
{
	pr_info("%s\n", __func__);
	gpio_direction_output(mdm_drv->ap2mdm_errfatal_gpio, 0);
	gpio_direction_output(mdm_drv->ap2mdm_status_gpio, 1);
	mdm_drv->ops->power_on_mdm_cb(mdm_drv);
	mdm_drv->boot_type = CHARM_NORMAL_BOOT;
	complete(&mdm_needs_reload);
	if (!wait_for_completion_timeout(&mdm_boot,
			msecs_to_jiffies(MDM_BOOT_TIMEOUT))) {
		mdm_drv->mdm_boot_status = -ETIMEDOUT;
		pr_info("%s: mdm modem restart timed out.\n", __func__);
	} else
		pr_info("%s: mdm modem has been restarted\n", __func__);
	INIT_COMPLETION(mdm_boot);
	return mdm_drv->mdm_boot_status;
}

static int mdm_subsys_ramdumps(int want_dumps,
				const struct subsys_data *crashed_subsys)
{
	pr_info("%s\n", __func__);
	mdm_drv->mdm_ram_dump_status = 0;
	if (want_dumps) {
		mdm_drv->boot_type = CHARM_RAM_DUMPS;
		complete(&mdm_needs_reload);
		if (!wait_for_completion_timeout(&mdm_ram_dumps,
				msecs_to_jiffies(MDM_RDUMP_TIMEOUT))) {
			mdm_drv->mdm_ram_dump_status = -ETIMEDOUT;
			pr_info("%s: mdm modem ramdumps timed out.\n",
					__func__);
		} else
			pr_info("%s: mdm modem ramdumps completed.\n",
					__func__);
		INIT_COMPLETION(mdm_ram_dumps);
		gpio_direction_output(mdm_drv->ap2mdm_errfatal_gpio, 1);
		mdm_drv->ops->power_down_mdm_cb(mdm_drv);
	}
	return mdm_drv->mdm_ram_dump_status;
}

static struct subsys_data mdm_subsystem = {
	.shutdown = mdm_subsys_shutdown,
	.ramdump = mdm_subsys_ramdumps,
	.powerup = mdm_subsys_powerup,
	.name = EXTERNAL_MODEM,
};

static int mdm_debug_on_set(void *data, u64 val)
{
	mdm_debug_on = val;
	if (mdm_drv->ops->debug_state_changed_cb)
		mdm_drv->ops->debug_state_changed_cb(mdm_debug_on);
	return 0;
}

static int mdm_debug_on_get(void *data, u64 *val)
{
	*val = mdm_debug_on;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mdm_debug_on_fops,
			mdm_debug_on_get,
			mdm_debug_on_set, "%llu\n");

static int mdm_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("mdm_dbg", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("debug_on", 0644, dent, NULL,
			&mdm_debug_on_fops);
	return 0;
}

static void mdm_modem_initialize_data(struct platform_device  *pdev,
				struct mdm_ops *mdm_ops)
{
	struct resource *pres;

	/* MDM2AP_ERRFATAL */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"MDM2AP_ERRFATAL");
	if (pres)
		mdm_drv->mdm2ap_errfatal_gpio = pres->start;

	/* AP2MDM_ERRFATAL */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"AP2MDM_ERRFATAL");
	if (pres)
		mdm_drv->ap2mdm_errfatal_gpio = pres->start;

	/* MDM2AP_STATUS */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"MDM2AP_STATUS");
	if (pres)
		mdm_drv->mdm2ap_status_gpio = pres->start;

	/* AP2MDM_STATUS */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"AP2MDM_STATUS");
	if (pres)
		mdm_drv->ap2mdm_status_gpio = pres->start;

	/* MDM2AP_WAKEUP */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"MDM2AP_WAKEUP");
	if (pres)
		mdm_drv->mdm2ap_wakeup_gpio = pres->start;

	/* AP2MDM_WAKEUP */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"AP2MDM_WAKEUP");
	if (pres)
		mdm_drv->ap2mdm_wakeup_gpio = pres->start;

	/* AP2MDM_PMIC_RESET_N */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"AP2MDM_PMIC_RESET_N");
	if (pres)
		mdm_drv->ap2mdm_pmic_reset_n_gpio = pres->start;

	/* AP2MDM_KPDPWR_N */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"AP2MDM_KPDPWR_N");
	if (pres)
		mdm_drv->ap2mdm_kpdpwr_n_gpio = pres->start;

	mdm_drv->boot_type                  = CHARM_NORMAL_BOOT;

	mdm_drv->ops      = mdm_ops;
	mdm_drv->pdata    = pdev->dev.platform_data;
}

int mdm_common_create(struct platform_device  *pdev,
					  struct mdm_ops *p_mdm_cb)
{
	int ret = -1, irq;
	pr_err("%s\n", __func__);

	mdm_drv = kzalloc(sizeof(struct mdm_modem_drv), GFP_KERNEL);
	if (mdm_drv == NULL) {
		pr_err("%s: kzalloc fail.\n", __func__);
		goto alloc_err;
	}

	mdm_modem_initialize_data(pdev, p_mdm_cb);
	if (mdm_drv->ops->debug_state_changed_cb)
		mdm_drv->ops->debug_state_changed_cb(mdm_debug_on);

	gpio_request(mdm_drv->ap2mdm_status_gpio, "AP2MDM_STATUS");
	gpio_request(mdm_drv->ap2mdm_errfatal_gpio, "AP2MDM_ERRFATAL");
	gpio_request(mdm_drv->ap2mdm_kpdpwr_n_gpio, "AP2MDM_KPDPWR_N");
	gpio_request(mdm_drv->ap2mdm_pmic_reset_n_gpio, "AP2MDM_PMIC_RESET_N");
	gpio_request(mdm_drv->mdm2ap_status_gpio, "MDM2AP_STATUS");
	gpio_request(mdm_drv->mdm2ap_errfatal_gpio, "MDM2AP_ERRFATAL");

	if (mdm_drv->ap2mdm_wakeup_gpio > 0)
		gpio_request(mdm_drv->ap2mdm_wakeup_gpio, "AP2MDM_WAKEUP");

#ifdef CONFIG_ARCH_EXYNOS
	gpio_set_value(mdm_drv->ap2mdm_status_gpio, 1);
	s3c_gpio_cfgpin(mdm_drv->ap2mdm_status_gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(mdm_drv->ap2mdm_status_gpio, S3C_GPIO_PULL_UP);
#endif
	gpio_direction_output(mdm_drv->ap2mdm_status_gpio, 1);
	pr_err("%s> : right after ap2mdm_status = %d\n", __func__,
				gpio_get_value(mdm_drv->ap2mdm_status_gpio));

#ifdef CONFIG_ARCH_EXYNOS
	gpio_set_value(mdm_drv->ap2mdm_errfatal_gpio, 0);
	s3c_gpio_cfgpin(mdm_drv->ap2mdm_errfatal_gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(mdm_drv->ap2mdm_errfatal_gpio, S3C_GPIO_PULL_DOWN);
#endif
	gpio_direction_output(mdm_drv->ap2mdm_errfatal_gpio, 0);
	pr_err("%s>> : right after ap2mdm_status = %d\n", __func__,
				gpio_get_value(mdm_drv->ap2mdm_status_gpio));

	if (mdm_drv->ap2mdm_wakeup_gpio > 0)
		gpio_direction_output(mdm_drv->ap2mdm_wakeup_gpio, 0);

	gpio_direction_input(mdm_drv->mdm2ap_status_gpio);
	gpio_direction_input(mdm_drv->mdm2ap_errfatal_gpio);

	mdm_queue = create_singlethread_workqueue("mdm_queue");
	if (!mdm_queue) {
		pr_err("%s: could not create workqueue. All mdm "
				"functionality will be disabled\n",
			__func__);
		ret = -ENOMEM;
		goto fatal_err;
	}

	atomic_notifier_chain_register(&panic_notifier_list, &mdm_panic_blk);
	mdm_debugfs_init();

	/* Register subsystem handlers */
	ssr_register_subsystem(&mdm_subsystem);

	/* ERR_FATAL irq. */
#ifdef CONFIG_ARCH_EXYNOS
	irq = gpio_to_irq(mdm_drv->mdm2ap_errfatal_gpio);
#else
	irq = MSM_GPIO_TO_INT(mdm_drv->mdm2ap_errfatal_gpio);
#endif
	if (irq < 0) {
		pr_err("%s: could not get MDM2AP_ERRFATAL IRQ resource. "
			"error=%d No IRQ will be generated on errfatal.",
			__func__, irq);
		goto errfatal_err;
	}
	ret = request_irq(irq, mdm_errfatal,
		IRQF_TRIGGER_RISING , "mdm errfatal", NULL);

	if (ret < 0) {
		pr_err("%s: MDM2AP_ERRFATAL IRQ#%d request failed with error=%d"
			". No IRQ will be generated on errfatal.",
			__func__, irq, ret);
		goto errfatal_err;
	}
	mdm_drv->mdm_errfatal_irq = irq;

errfatal_err:

	/* status irq */
#ifdef CONFIG_ARCH_EXYNOS
	ret = s5p_register_gpio_interrupt(mdm_drv->mdm2ap_status_gpio);
	if (ret)
		pr_err("%s: register MDM2AP_STATUS ret = %d\n", __func__, ret);
	irq = gpio_to_irq(mdm_drv->mdm2ap_status_gpio);
#else
	irq = MSM_GPIO_TO_INT(mdm_drv->mdm2ap_status_gpio);
#endif
	if (irq < 0) {
		pr_err("%s: could not get MDM2AP_STATUS IRQ resource. "
			"error=%d No IRQ will be generated on status change.",
			__func__, irq);
		goto status_err;
	}

	ret = request_threaded_irq(irq, NULL, mdm_status_change,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_SHARED,
		"mdm status", mdm_drv);

	if (ret < 0) {
		pr_err("%s: MDM2AP_STATUS IRQ#%d request failed with error=%d"
			". No IRQ will be generated on status change.",
			__func__, irq, ret);
		goto status_err;
	}
	mdm_drv->mdm_status_irq = irq;

status_err:
	/* Perform early powerup of the external modem in order to
	 * allow tabla devices to be found.
	 */
	mdm_drv->ops->power_on_mdm_cb(mdm_drv);
	pr_err("%s : ap2mdm_status = %d\n", __func__,
				gpio_get_value(mdm_drv->ap2mdm_status_gpio));

	pr_info("%s: Registering mdm modem\n", __func__);
	return misc_register(&mdm_modem_misc);

fatal_err:
	gpio_free(mdm_drv->ap2mdm_status_gpio);
	gpio_free(mdm_drv->ap2mdm_errfatal_gpio);
	gpio_free(mdm_drv->ap2mdm_kpdpwr_n_gpio);
	gpio_free(mdm_drv->ap2mdm_pmic_reset_n_gpio);
	gpio_free(mdm_drv->mdm2ap_status_gpio);
	gpio_free(mdm_drv->mdm2ap_errfatal_gpio);

	if (mdm_drv->ap2mdm_wakeup_gpio > 0)
		gpio_free(mdm_drv->ap2mdm_wakeup_gpio);

	kfree(mdm_drv);
	ret = -ENODEV;

alloc_err:
	return ret;
}

int mdm_common_modem_remove(struct platform_device *pdev)
{
	int ret;

	gpio_free(mdm_drv->ap2mdm_status_gpio);
	gpio_free(mdm_drv->ap2mdm_errfatal_gpio);
	gpio_free(mdm_drv->ap2mdm_kpdpwr_n_gpio);
	gpio_free(mdm_drv->ap2mdm_pmic_reset_n_gpio);
	gpio_free(mdm_drv->mdm2ap_status_gpio);
	gpio_free(mdm_drv->mdm2ap_errfatal_gpio);

	if (mdm_drv->ap2mdm_wakeup_gpio > 0)
		gpio_free(mdm_drv->ap2mdm_wakeup_gpio);

	kfree(mdm_drv);

	ret = misc_deregister(&mdm_modem_misc);
	return ret;
}

void mdm_common_modem_shutdown(struct platform_device *pdev)
{
	mdm_disable_irqs();

	mdm_drv->ops->power_down_mdm_cb(mdm_drv);
}

