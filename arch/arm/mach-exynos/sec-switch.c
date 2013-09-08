#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/gpio_event.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/input.h>
#include <plat/udc-hs.h>
/*#include <linux/mmc/host.h>*/
#include <linux/regulator/machine.h>
#include <linux/regulator/max8649.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>
#include <linux/mfd/max77686.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/gpio.h>

#include <linux/power_supply.h>
#include <linux/battery/samsung_battery.h>

#include <linux/switch.h>
#include <linux/sii9234.h>

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif
#include <linux/pm_runtime.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#ifdef CONFIG_JACK_MON
#include <linux/jack.h>
#endif

#ifdef CONFIG_MACH_SLP_NAPLES
#include <mach/naples-tsp.h>
#endif
#ifdef CONFIG_MACH_MIDAS
#include <linux/platform_data/mms_ts.h>
#endif

static struct switch_dev switch_dock = {
	.name = "dock",
};

extern struct class *sec_class;

struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

/* charger cable state */
bool is_cable_attached;
bool is_jig_attached;

static ssize_t midas_switch_show_vbus(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int i;
	struct regulator *regulator;

	regulator = regulator_get(NULL, "safeout1");
	if (IS_ERR(regulator)) {
		pr_warn("%s: fail to get regulator\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	if (regulator_is_enabled(regulator))
		i = sprintf(buf, "VBUS is enabled\n");
	else
		i = sprintf(buf, "VBUS is disabled\n");
	regulator_put(regulator);

	return i;
}

static ssize_t midas_switch_store_vbus(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int disable, ret, usb_mode;
	struct regulator *regulator;
	/* struct s3c_udc *udc = platform_get_drvdata(&s3c_device_usbgadget); */

	if (!strncmp(buf, "0", 1))
		disable = 0;
	else if (!strncmp(buf, "1", 1))
		disable = 1;
	else {
		pr_warn("%s: Wrong command\n", __func__);
		return count;
	}

	pr_info("%s: disable=%d\n", __func__, disable);
	usb_mode =
	    disable ? USB_CABLE_DETACHED_WITHOUT_NOTI : USB_CABLE_ATTACHED;
	/* ret = udc->change_usb_mode(usb_mode); */
	ret = -1;
	if (ret < 0)
		pr_err("%s: fail to change mode!!!\n", __func__);

	regulator = regulator_get(NULL, "safeout1");
	if (IS_ERR(regulator)) {
		pr_warn("%s: fail to get regulator\n", __func__);
		return count;
	}

	if (disable) {
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
	} else {
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
	}
	regulator_put(regulator);

	return count;
}

DEVICE_ATTR(disable_vbus, 0664, midas_switch_show_vbus,
	    midas_switch_store_vbus);

static int __init midas_sec_switch_init(void)
{
	int ret;
	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");

	if (IS_ERR(switch_dev))
		pr_err("Failed to create device(switch)!\n");

	ret = device_create_file(switch_dev, &dev_attr_disable_vbus);
	if (ret)
		pr_err("Failed to create device file(disable_vbus)!\n");

	return 0;
};

int max77693_muic_charger_cb(enum cable_type_muic cable_type)
{
#ifdef CONFIG_BATTERY_MAX77693_CHARGER
	struct power_supply *psy = power_supply_get_by_name("max77693-charger");
	union power_supply_propval value;
#endif
	pr_info("%s: %d\n", __func__, cable_type);

	switch (cable_type) {
	case CABLE_TYPE_NONE_MUIC:
	case CABLE_TYPE_OTG_MUIC:
	case CABLE_TYPE_JIG_UART_OFF_MUIC:
	case CABLE_TYPE_MHL_MUIC:
		is_cable_attached = false;
		break;
	case CABLE_TYPE_USB_MUIC:
	case CABLE_TYPE_JIG_USB_OFF_MUIC:
	case CABLE_TYPE_JIG_USB_ON_MUIC:
		is_cable_attached = true;
		break;
	case CABLE_TYPE_MHL_VB_MUIC:
		is_cable_attached = true;
		break;
	case CABLE_TYPE_TA_MUIC:
	case CABLE_TYPE_CARDOCK_MUIC:
	case CABLE_TYPE_DESKDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_MUIC:
	case CABLE_TYPE_JIG_UART_OFF_VB_MUIC:
		is_cable_attached = true;
		break;
	default:
		pr_err("%s: invalid type:%d\n", __func__, cable_type);
		return -EINVAL;
	}

#ifdef CONFIG_BATTERY_MAX77693_CHARGER
	if (!psy) {
		pr_err("%s: fail to get max77693-charger psy\n", __func__);
		return 0;
	}

	value.intval = cable_type;
	psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
#endif

#if defined(CONFIG_MACH_SLP_NAPLES) || defined(CONFIG_MACH_MIDAS)
#ifndef CONFIG_MACH_GC1
	tsp_charger_infom(is_cable_attached);
#endif
#endif
#ifdef CONFIG_JACK_MON
	jack_event_handler("charger", is_cable_attached);
#endif

	return 0;
}

int max77693_get_jig_state(void)
{
	pr_info("%s: %d\n", __func__, is_jig_attached);
	return is_jig_attached;
}
EXPORT_SYMBOL(max77693_get_jig_state);

void max77693_set_jig_state(int jig_state)
{
	pr_info("%s: %d\n", __func__, jig_state);
	is_jig_attached = jig_state;
}

/* usb cable call back function */
void max77693_muic_usb_cb(u8 usb_mode)
{
	struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usbgadget);
#ifdef CONFIG_USB_HOST_NOTIFY
	struct host_notifier_platform_data *host_noti_pdata =
	    host_notifier_device.dev.platform_data;
#endif

	pr_info("MUIC usb_cb:%d\n", usb_mode);
	if (gadget) {
		switch (usb_mode) {
		case USB_CABLE_DETACHED:
			pr_info("usb: muic: USB_CABLE_DETACHED(%d)\n",
				usb_mode);
			usb_gadget_vbus_disconnect(gadget);
			break;
		case USB_CABLE_ATTACHED:
			pr_info("usb: muic: USB_CABLE_ATTACHED(%d)\n",
				usb_mode);
			usb_gadget_vbus_connect(gadget);
			break;
		default:
			pr_info("usb: muic: invalid mode%d\n", usb_mode);
		}
	}

	if (usb_mode == USB_OTGHOST_ATTACHED
		|| usb_mode == USB_POWERED_HOST_ATTACHED) {
#ifdef CONFIG_USB_HOST_NOTIFY
		if (usb_mode == USB_OTGHOST_ATTACHED)
			host_noti_pdata->booster(1);
		else
			host_noti_pdata->powered_booster(1);

		host_noti_pdata->ndev.mode = NOTIFY_HOST_MODE;
		if (host_noti_pdata->usbhostd_start)
			host_noti_pdata->usbhostd_start();
#endif
#ifdef CONFIG_USB_EHCI_S5P
		pm_runtime_get_sync(&s5p_device_ehci.dev);
#endif
#ifdef CONFIG_USB_OHCI_S5P
		pm_runtime_get_sync(&s5p_device_ohci.dev);
#endif
	} else if (usb_mode == USB_OTGHOST_DETACHED
		|| usb_mode == USB_POWERED_HOST_DETACHED) {
#ifdef CONFIG_USB_OHCI_S5P
		pm_runtime_put_sync(&s5p_device_ohci.dev);
#endif
#ifdef CONFIG_USB_EHCI_S5P
		pm_runtime_put_sync(&s5p_device_ehci.dev);
#endif
#ifdef CONFIG_USB_HOST_NOTIFY
		host_noti_pdata->ndev.mode = NOTIFY_NONE_MODE;
		if (host_noti_pdata->usbhostd_stop)
			host_noti_pdata->usbhostd_stop();
		if (usb_mode == USB_OTGHOST_DETACHED)
			host_noti_pdata->booster(0);
		else
			host_noti_pdata->powered_booster(0);
#endif
	}

#ifdef CONFIG_JACK_MON
	if (usb_mode == USB_OTGHOST_ATTACHED
	|| usb_mode == USB_POWERED_HOST_ATTACHED)
		jack_event_handler("host", USB_CABLE_ATTACHED);
	else if (usb_mode == USB_OTGHOST_DETACHED
	|| usb_mode == USB_POWERED_HOST_DETACHED)
		jack_event_handler("host", USB_CABLE_DETACHED);
	else if ((usb_mode == USB_CABLE_ATTACHED)
		|| (usb_mode == USB_CABLE_DETACHED))
		jack_event_handler("usb", usb_mode);
#endif
}

/*extern void MHL_On(bool on);*/
void max77693_muic_mhl_cb(int attached)
{
	pr_info("MUIC attached:%d\n", attached);
	if (attached == MAX77693_MUIC_ATTACHED) {
		/*MHL_On(1);*/ /* GPIO_LEVEL_HIGH */
		pr_info("MHL Attached !!\n");
#ifdef CONFIG_SAMSUNG_MHL
#ifdef CONFIG_MACH_MIDAS
		sii9234_wake_lock();
#endif
		mhl_onoff_ex(1);
#endif
	} else {
		/*MHL_On(0);*/ /* GPIO_LEVEL_LOW */
		pr_info("MHL Detached !!\n");
#ifdef CONFIG_SAMSUNG_MHL
		mhl_onoff_ex(false);
#ifdef CONFIG_MACH_MIDAS
		sii9234_wake_unlock();
#endif
#endif
	}
}

bool max77693_muic_is_mhl_attached(void)
{
	int val;
#ifdef CONFIG_SAMSUNG_USE_11PIN_CONNECTOR
	val = max77693_muic_get_status1_adc1k_value();
	pr_info("%s(1): %d\n", __func__, val);
	return val;
#else
	const int err = -1;
	int ret;

	ret = gpio_request(GPIO_MHL_SEL, "MHL_SEL");
	if (ret) {
			pr_err("fail to request gpio %s\n", "GPIO_MHL_SEL");
			return err;
	}
	val = gpio_get_value(GPIO_MHL_SEL);
	pr_info("%s(2): %d\n", __func__, val);
	gpio_free(GPIO_MHL_SEL);
	return !!val;
#endif
}

void max77693_muic_deskdock_cb(bool attached)
{
	pr_info("MUIC deskdock attached=%d\n", attached);
	if (attached) {
#ifdef CONFIG_JACK_MON
		jack_event_handler("cradle", 1);
#endif
		switch_set_state(&switch_dock, 1);
	} else {
#ifdef CONFIG_JACK_MON
		jack_event_handler("cradle", 0);
#endif
		switch_set_state(&switch_dock, 0);
	}
}

void max77693_muic_cardock_cb(bool attached)
{
	pr_info("MUIC cardock attached=%d\n", attached);
	pr_info("##MUIC [ %s ]- func : %s !!\n", __FILE__, __func__);
	if (attached) {
#ifdef CONFIG_JACK_MON
		jack_event_handler("cradle", 2);
#endif
		switch_set_state(&switch_dock, 2);
	} else {
#ifdef CONFIG_JACK_MON
		jack_event_handler("cradle", 0);
#endif
		switch_set_state(&switch_dock, 0);
	}
}

void max77693_muic_init_cb(void)
{
	int ret;

	/* for CarDock, DeskDock */
	ret = switch_dev_register(&switch_dock);

	pr_info("MUIC ret=%d\n", ret);

	if (ret < 0)
		pr_err("Failed to register dock switch. %d\n", ret);
}

int max77693_muic_cfg_uart_gpio(void)
{
	int uart_val, path;
	pr_info("## MUIC func : %s ! please  path: (uart:%d - usb:%d)\n",
		__func__, gpio_get_value(GPIO_UART_SEL),
		gpio_get_value(GPIO_USB_SEL));
	uart_val = gpio_get_value(GPIO_UART_SEL);
	path = uart_val ? UART_PATH_AP : UART_PATH_CP;
#ifdef CONFIG_LTE_VIA_SWITCH
	if (path == UART_PATH_CP && !gpio_get_value(GPIO_LTE_VIA_UART_SEL))
		path = UART_PATH_LTE;
#endif
	pr_info("##MUIC [ %s ]- func : %s! path:%d\n", __FILE__, __func__,
		path);
	return path;
}

void max77693_muic_jig_uart_cb(int path)
{
	pr_info("func:%s : (path=%d\n", __func__, path);
	switch (path) {
	case UART_PATH_AP:
		gpio_set_value(GPIO_UART_SEL, GPIO_LEVEL_HIGH);
		break;
	case UART_PATH_CP:
		gpio_set_value(GPIO_UART_SEL, GPIO_LEVEL_LOW);
#ifdef CONFIG_LTE_VIA_SWITCH
		gpio_set_value(GPIO_LTE_VIA_UART_SEL, GPIO_LEVEL_HIGH);
#endif
		break;
#ifdef CONFIG_LTE_VIA_SWITCH
	case UART_PATH_LTE:
		gpio_set_value(GPIO_UART_SEL, GPIO_LEVEL_LOW);
		gpio_set_value(GPIO_LTE_VIA_UART_SEL, GPIO_LEVEL_LOW);
		break;
#endif
	default:
		pr_info("func %s: invalid value!!\n", __func__);
	}

}

#ifdef CONFIG_USB_HOST_NOTIFY
int max77693_muic_host_notify_cb(int enable)
{
	struct host_notifier_platform_data *host_noti_pdata =
	    host_notifier_device.dev.platform_data;

	struct host_notify_dev *ndev = &host_noti_pdata->ndev;

	if (!ndev) {
		pr_err("%s: ndev is null.\n", __func__);
		return -1;
	}

	ndev->booster = enable ? NOTIFY_POWER_ON : NOTIFY_POWER_OFF;
	pr_info("%s: mode %d, enable %d\n", __func__, ndev->mode, enable);
	return ndev->mode;
}
#endif

int max77693_muic_set_safeout(int path)
{
	struct regulator *regulator;

	pr_info("MUIC safeout path=%d\n", path);

	if (path == CP_USB_MODE) {
		regulator = regulator_get(NULL, "safeout1");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);

		regulator = regulator_get(NULL, "safeout2");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		/* AP_USB_MODE || AUDIO_MODE */
		regulator = regulator_get(NULL, "safeout1");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
		regulator_put(regulator);

		regulator = regulator_get(NULL, "safeout2");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}

	return 0;
}

struct max77693_muic_data max77693_muic = {
	.usb_cb = max77693_muic_usb_cb,
	.charger_cb = max77693_muic_charger_cb,
	.mhl_cb = max77693_muic_mhl_cb,
	.is_mhl_attached = max77693_muic_is_mhl_attached,
	.set_safeout = max77693_muic_set_safeout,
	.init_cb = max77693_muic_init_cb,
	.deskdock_cb = max77693_muic_deskdock_cb,
	.cardock_cb = max77693_muic_cardock_cb,
#if !defined(CONFIG_MACH_GC1)
	.cfg_uart_gpio = max77693_muic_cfg_uart_gpio,
	.jig_uart_cb = max77693_muic_jig_uart_cb,
#endif /* CONFIG_MACH_GC1 */
#ifdef CONFIG_USB_HOST_NOTIFY
	.host_notify_cb = max77693_muic_host_notify_cb,
#else
	.host_notify_cb = NULL,
#endif
#if !defined(CONFIG_MACH_GC1)
	.gpio_usb_sel = GPIO_USB_SEL,
#else
	.gpio_usb_sel = -1,
#endif /* CONFIG_MACH_GC1 */
	.jig_state = max77693_set_jig_state,
};

device_initcall(midas_sec_switch_init);
