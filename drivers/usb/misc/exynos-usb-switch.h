/*
 * exynos-usb-switch.h - USB switch driver for Exynos
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 * Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __EXYNOS_USB_SWITCH
#define __EXYNOS_USB_SWITCH

#define SWITCH_WAIT_TIME	500
#define WAIT_TIMES		10

enum usb_cable_status {
	USB_DEVICE_ATTACHED,
	USB_HOST_ATTACHED,
	USB_DRD_DEVICE_ATTACHED,
	USB_DRD_HOST_ATTACHED,
	USB_DEVICE_DETACHED,
	USB_HOST_DETACHED,
	USB_DRD_DEVICE_DETACHED,
	USB_DRD_HOST_DETACHED,
};

struct exynos_usb_switch {
	unsigned long connect;

	unsigned int host_detect_irq;
	unsigned int device_detect_irq;
	unsigned int host_drd_detect_irq;
	unsigned int device_drd_detect_irq;
	unsigned int gpio_host_detect;
	unsigned int gpio_device_detect;
	unsigned int gpio_host_vbus;
	unsigned int gpio_drd_host_detect;
	unsigned int gpio_drd_device_detect;

	struct device *ehci_dev;
	struct device *ohci_dev;
	struct device *xhci_dev;

	struct device *s3c_udc_dev;
	struct device *exynos_udc_dev;

	struct workqueue_struct	*workqueue;
	struct work_struct switch_work;
	struct work_struct switch_drd_work;
	struct mutex mutex;
	atomic_t usb_status;
	int (*get_usb_mode)(void);
	int (*change_usb_mode)(int mode);
};

extern int s5p_ehci_port_power_off(struct platform_device *pdev);
extern int s5p_ohci_port_power_off(struct platform_device *pdev);

extern int s5p_ehci_port_power_on(struct platform_device *pdev);
extern int s5p_ohci_port_power_on(struct platform_device *pdev);

#endif
