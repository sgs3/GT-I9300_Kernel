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
#include "devices.h"
#include "clock.h"
#endif
#include "mdm_private.h"
#include <linux/wakelock.h>

#define MDM_MODEM_TIMEOUT	6000
#define MDM_HOLD_TIME		4000
#define MDM_MODEM_DELTA		100

static int mdm_debug_on;
static int power_on_count;
static int hsic_peripheral_status = 1;
static DEFINE_MUTEX(hsic_status_lock);

static void mdm_peripheral_connect(struct mdm_modem_drv *mdm_drv)
{
	pr_err("%s\n", __func__);
	mutex_lock(&hsic_status_lock);
	if (hsic_peripheral_status)
		goto out;
	if (mdm_drv->pdata->peripheral_platform_device)
		platform_device_add(mdm_drv->pdata->peripheral_platform_device);
	hsic_peripheral_status = 1;
out:
	mutex_unlock(&hsic_status_lock);
	pr_err("%s : ap2mdm_status = %d\n", __func__,
				gpio_get_value(mdm_drv->ap2mdm_status_gpio));
}

static void mdm_peripheral_disconnect(struct mdm_modem_drv *mdm_drv)
{
	pr_err("%s\n", __func__);
	mutex_lock(&hsic_status_lock);
	if (!hsic_peripheral_status)
		goto out;
	if (mdm_drv->pdata->peripheral_platform_device)
		platform_device_del(mdm_drv->pdata->peripheral_platform_device);
	hsic_peripheral_status = 0;
out:
	mutex_unlock(&hsic_status_lock);
	pr_err("%s : ap2mdm_status = %d\n", __func__,
				gpio_get_value(mdm_drv->ap2mdm_status_gpio));
}

static void power_on_mdm(struct mdm_modem_drv *mdm_drv)
{
	power_on_count++;

	pr_err("%s: power count %d\n", __func__, power_on_count);
	/* this gpio will be used to indicate apq readiness,
	 * de-assert it now so that it can asserted later
	 */
	gpio_direction_output(mdm_drv->ap2mdm_wakeup_gpio, 0);

	/* The second attempt to power-on the mdm is the first attempt
	 * from user space, but we're already powered on. Ignore this.
	 * Subsequent attempts are from SSR or if something failed, in
	 * which case we must always reset the modem.
	 */
	if (power_on_count == 2)
		return;

	mdm_peripheral_disconnect(mdm_drv);

	/* Pull RESET gpio low and wait for it to settle. */
	pr_info("Pulling RESET gpio low\n");
	gpio_direction_output(mdm_drv->ap2mdm_pmic_reset_n_gpio, 0);
	usleep_range(5000, 10000);

	/* Deassert RESET first and wait for ir to settle. */
	pr_info("%s: Pulling RESET gpio high\n", __func__);
	gpio_direction_output(mdm_drv->ap2mdm_pmic_reset_n_gpio, 1);
	msleep(20);

	/* Pull PWR gpio high and wait for it to settle, but only
	 * the first time the mdm is powered up.
	 * Some targets do not use ap2mdm_kpdpwr_n_gpio.
	 */
	if (power_on_count == 1) {
		if (mdm_drv->ap2mdm_kpdpwr_n_gpio > 0) {
			pr_debug("%s: Powering on mdm modem\n", __func__);
			gpio_direction_output(mdm_drv->ap2mdm_kpdpwr_n_gpio, 1);
			usleep_range(1000, 1000);
		}
	}

#ifdef CONFIG_ARCH_EXYNOS
	gpio_direction_output(mdm_drv->ap2mdm_status_gpio, 1);
#endif
	mdm_peripheral_connect(mdm_drv);

	msleep(200);
}

static void power_down_mdm(struct mdm_modem_drv *mdm_drv)
{
	int i;

	pr_err("%s\n", __func__);
	for (i = MDM_MODEM_TIMEOUT; i > 0; i -= MDM_MODEM_DELTA) {
		/* pet_watchdog(); */
		msleep(MDM_MODEM_DELTA);
		if (gpio_get_value(mdm_drv->mdm2ap_status_gpio) == 0)
			break;
	}
	if (i <= 0) {
		pr_err("%s: MDM2AP_STATUS never went low.\n",
			 __func__);
		gpio_direction_output(mdm_drv->ap2mdm_pmic_reset_n_gpio, 0);

		for (i = MDM_HOLD_TIME; i > 0; i -= MDM_MODEM_DELTA) {
			/* pet_watchdog(); */
			msleep(MDM_MODEM_DELTA);
		}
	}
	if (mdm_drv->ap2mdm_kpdpwr_n_gpio > 0)
		gpio_direction_output(mdm_drv->ap2mdm_kpdpwr_n_gpio, 0);
	mdm_peripheral_disconnect(mdm_drv);
}

#ifdef CONFIG_ARCH_EXYNOS
static void normal_boot_done(struct mdm_modem_drv *mdm_drv)
{
	pr_err("%s\n", __func__);
	mdm_peripheral_disconnect(mdm_drv);
}
#endif

static void debug_state_changed(int value)
{
	mdm_debug_on = value;
}

static void mdm_status_changed(struct mdm_modem_drv *mdm_drv, int value)
{
	pr_debug("%s: value:%d\n", __func__, value);

	pr_err("%s: ap2mdm_status = %d\n", __func__,
				gpio_get_value(mdm_drv->ap2mdm_status_gpio));
	if (value) {
		mdm_peripheral_disconnect(mdm_drv);
		mdm_peripheral_connect(mdm_drv);
		gpio_direction_output(mdm_drv->ap2mdm_wakeup_gpio, 1);
	}
}

static struct mdm_ops mdm_cb = {
	.power_on_mdm_cb = power_on_mdm,
	.power_down_mdm_cb = power_down_mdm,
	.debug_state_changed_cb = debug_state_changed,
	.status_cb = mdm_status_changed,
#ifdef CONFIG_ARCH_EXYNOS
	.normal_boot_done_cb = normal_boot_done,
#endif
};

/* temprary wakelock, remove when L3 state implemented */
#ifdef CONFIG_ARCH_EXYNOS
static struct wake_lock mdm_wake;
#endif

static int __init mdm_modem_probe(struct platform_device *pdev)
{
	pr_err("%s\n", __func__);
/* temprary wakelock, remove when L3 state implemented */
#ifdef CONFIG_ARCH_EXYNOS
	wake_lock_init(&mdm_wake, WAKE_LOCK_SUSPEND, "mdm_wake");
	wake_lock(&mdm_wake);
#endif
	return mdm_common_create(pdev, &mdm_cb);
}

static int __devexit mdm_modem_remove(struct platform_device *pdev)
{
	return mdm_common_modem_remove(pdev);
}

static void mdm_modem_shutdown(struct platform_device *pdev)
{
	mdm_common_modem_shutdown(pdev);
}

static struct platform_driver mdm_modem_driver = {
	.remove         = mdm_modem_remove,
	.shutdown	= mdm_modem_shutdown,
	.driver         = {
		.name = "mdm2_modem",
		.owner = THIS_MODULE
	},
};

static int __init mdm_modem_init(void)
{
	return platform_driver_probe(&mdm_modem_driver, mdm_modem_probe);
}

static void __exit mdm_modem_exit(void)
{
	platform_driver_unregister(&mdm_modem_driver);
}

late_initcall(mdm_modem_init);
/* module_init(mdm_modem_init); */
module_exit(mdm_modem_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("mdm modem driver");
MODULE_VERSION("2.0");
MODULE_ALIAS("mdm_modem");
