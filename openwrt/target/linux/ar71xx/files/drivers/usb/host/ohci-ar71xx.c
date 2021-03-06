/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 * Copyright (C) 2007 Atheros Communications, Inc.
 * Copyright (C) 2008 Gabor Juhos <juhosg@openwrt.org>
 * Copyright (C) 2008 Imre Kaloz <kaloz@openwrt.org>
 *
 * Bus Glue for Atheros AR71xx built-in OHCI controller
 *
 */

#include <linux/platform_device.h>
#include <linux/delay.h>

extern int usb_disabled(void);

static void ar71xx_start_ohci(struct platform_device *pdev)
{
}

static void ar71xx_stop_ohci(struct platform_device *pdev)
{
	/*
	 * TODO: implement
	 */
}

int usb_hcd_ar71xx_probe(const struct hc_driver *driver,
			 struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct resource *res;
	int irq;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_dbg(&pdev->dev, "no IRQ specified for %s\n",
			pdev->dev.bus_id);
		return -ENODEV;
	}
	irq = res->start;

	hcd = usb_create_hcd(driver, &pdev->dev, pdev->dev.bus_id);
	if (!hcd)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_dbg(&pdev->dev, "no base address specified for %s\n",
			pdev->dev.bus_id);
		ret = -ENODEV;
		goto err_put_hcd;
	}
	hcd->rsrc_start	= res->start;
	hcd->rsrc_len	= res->end - res->start + 1;

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len, hcd_name)) {
		dev_dbg(&pdev->dev, "controller already in use\n");
		ret = -EBUSY;
		goto err_put_hcd;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		dev_dbg(&pdev->dev, "error mapping memory\n");
		ret = -EFAULT;
		goto err_release_region;
	}

	ar71xx_start_ohci(pdev);
	ohci_hcd_init(hcd_to_ohci(hcd));

	ret = usb_add_hcd(hcd, irq, IRQF_DISABLED);
	if (ret)
		goto err_stop_hcd;

	return 0;

err_stop_hcd:
	ar71xx_stop_ohci(pdev);
	iounmap(hcd->regs);
err_release_region:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err_put_hcd:
	usb_put_hcd(hcd);
	return ret;
}


/* may be called without controller electrically present */
/* may be called with controller, bus, and devices active */

/**
 * usb_hcd_ar71xx_remove - shutdown processing for AR71xx HCDs
 * @dev: USB Host Controller being removed
 * Context: !in_interrupt()
 *
 * Reverses the effect of usb_hcd_ar71xx_probe(), first invoking
 * the HCD's stop() method.  It is always called from a thread
 * context, normally "rmmod", "apmd", or something similar.
 *
 */
void usb_hcd_ar71xx_remove(struct usb_hcd *hcd, struct platform_device *pdev)
{
	usb_remove_hcd(hcd);
	ar71xx_stop_ohci(pdev);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
}

static int __devinit ohci_ar71xx_start(struct usb_hcd *hcd)
{
	struct ohci_hcd	*ohci = hcd_to_ohci(hcd);
	int ret;

	ret = ohci_init(ohci);
	if (ret < 0)
		return ret;

	ret = ohci_run(ohci);
	if (ret < 0)
		goto err;

	return 0;

err:
	ohci_stop(hcd);
	return ret;
}

static const struct hc_driver ohci_ar71xx_hc_driver = {
	.description	= hcd_name,
	.product_desc	= "Atheros AR71xx built-in OHCI controller",
	.hcd_priv_size	= sizeof(struct ohci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq		= ohci_irq,
	.flags		= HCD_USB11 | HCD_MEMORY,

	/*
	 * basic lifecycle operations
	 */
	.start		= ohci_ar71xx_start,
	.stop		= ohci_stop,
	.shutdown	= ohci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue		= ohci_urb_enqueue,
	.urb_dequeue		= ohci_urb_dequeue,
	.endpoint_disable	= ohci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number	= ohci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data	= ohci_hub_status_data,
	.hub_control		= ohci_hub_control,
	.start_port_reset	= ohci_start_port_reset,
};

static int ohci_hcd_ar71xx_drv_probe(struct platform_device *pdev)
{
	int ret;

	ret = -ENODEV;
	if (!usb_disabled())
		ret = usb_hcd_ar71xx_probe(&ohci_ar71xx_hc_driver, pdev);

	return ret;
}

static int ohci_hcd_ar71xx_drv_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_hcd_ar71xx_remove(hcd, pdev);
	return 0;
}

static struct platform_driver ohci_hcd_ar71xx_driver = {
	.probe		= ohci_hcd_ar71xx_drv_probe,
	.remove		= ohci_hcd_ar71xx_drv_remove,
	.shutdown	= usb_hcd_platform_shutdown,
	.driver		= {
		.name	= "ar71xx-ohci",
		.owner	= THIS_MODULE,
	},
};

MODULE_ALIAS("ar71xx-ohci");
