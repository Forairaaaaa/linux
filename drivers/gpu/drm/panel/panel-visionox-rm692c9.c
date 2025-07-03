// SPDX-License-Identifier: GPL-2.0
// https://oshwhub.com/ccrs/g1392fh101gg-003-qu-dong-ban
// https://github.com/Cjiio/Linux-Kit-Core/blob/develop/u-boot-2021.10/include/cvitek/cvi_panels/dsi_visionox_rm692c9.h#L43
// https://github.com/raspberrypi/linux/blob/rpi-6.12.y/drivers/gpu/drm/panel/panel-waveshare-dsi-v2.c
// https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/panel/panel-visionox-rm69299.c

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

struct visionox_rm692c9_panel {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
};

static inline struct visionox_rm692c9_panel *
to_visionox_rm692c9_panel(struct drm_panel *panel)
{
	return container_of(panel, struct visionox_rm692c9_panel, panel);
}

static void visionox_rm692c9_panel_reset(struct visionox_rm692c9_panel *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(200);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(120);
}

static void visionox_rm692c9_panel_on(struct mipi_dsi_multi_context *dsi_ctx)
{
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xFE, 0x00);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xC2, 0x08);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0x35, 0x00);

	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0x51, 0x07, 0xFF);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0x11);
	msleep(120);

	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0x29);
	msleep(80);
}

static void visionox_rm692c9_panel_off(struct visionox_rm692c9_panel *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);

	msleep(120);

	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);

	msleep(120);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
}

static int visionox_rm692c9_panel_prepare(struct drm_panel *panel)
{
	struct visionox_rm692c9_panel *ctx = to_visionox_rm692c9_panel(panel);
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	visionox_rm692c9_panel_reset(ctx);
	visionox_rm692c9_panel_on(&dsi_ctx);

	return 0;
}

static int visionox_rm692c9_panel_unprepare(struct drm_panel *panel)
{
	struct visionox_rm692c9_panel *ctx = to_visionox_rm692c9_panel(panel);

	visionox_rm692c9_panel_off(ctx);

	return 0;
}

static const struct drm_display_mode panel_mode = {
	.clock = 83333,
	.hdisplay = 1080,
	.hsync_start = 1080 + 28, // HFP
	.hsync_end = 1080 + 28 + 4, // HSW
	.htotal = 1080 + 28 + 4 + 36, // HFP + HSW + HBP

	.vdisplay = 1240,
	.vsync_start = 1240 + 16, // VFP
	.vsync_end = 1240 + 16 + 4, // VSW
	.vtotal = 1240 + 16 + 4 + 8, // VFP + VSW + VBP

	.width_mm = 65,
	.height_mm = 75,
};

static int visionox_rm692c9_panel_get_modes(struct drm_panel *panel,
					    struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &panel_mode);
	if (!mode) {
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs visionox_rm692c9_panel_funcs = {
	.prepare = visionox_rm692c9_panel_prepare,
	.unprepare = visionox_rm692c9_panel_unprepare,
	.get_modes = visionox_rm692c9_panel_get_modes,
};

static int visionox_rm692c9_panel_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = bl->props.brightness;

	u8 payload[2] = {
		brightness >> 8,
		brightness & 0xff,
	};

	return mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_DISPLAY_BRIGHTNESS, payload,
				  sizeof(payload));
}

static int visionox_rm692c9_panel_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness;
	int ret;

	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	if (ret < 0) {
		return ret;
	}

	return brightness;
}

static const struct backlight_ops visionox_rm692c9_panel_bl_ops = {
	.update_status = visionox_rm692c9_panel_bl_update_status,
	.get_brightness = visionox_rm692c9_panel_bl_get_brightness,
};

static struct backlight_device *
visionox_rm692c9_panel_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 2047,
		.max_brightness = 2047,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &visionox_rm692c9_panel_bl_ops,
					      &props);
}

static int visionox_rm692c9_panel_probe(struct mipi_dsi_device *dsi)
{
	struct visionox_rm692c9_panel *ctx;
	int ret;

	dev_info(&dsi->dev, "dsi panel: %s\n",
		 (char *)of_get_property(dsi->dev.of_node, "compatible", NULL));

	ctx = devm_kzalloc(&dsi->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		return -ENOMEM;
	}
	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dsi = dsi;

	ctx->panel.prepare_prev_first = true;
	drm_panel_init(&ctx->panel, &dsi->dev, &visionox_rm692c9_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ctx->reset_gpio =
		devm_gpiod_get_optional(&dsi->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		return dev_err_probe(&dsi->dev, PTR_ERR(ctx->reset_gpio),
				     "Couldn't get our reset GPIO\n");
	}

	ctx->panel.backlight = visionox_rm692c9_panel_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight)) {
		ret = PTR_ERR(ctx->panel.backlight);
		dev_err(&dsi->dev, "Failed to create backlight: %d\n", ret);
		return ret;
	}

	drm_panel_add(&ctx->panel);

	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->lanes = 4;
	dev_info(&dsi->dev, "lanes: %d\n", dsi->lanes);

	ret = mipi_dsi_attach(dsi);
	if (ret) {
		drm_panel_remove(&ctx->panel);
	}

	return ret;
}

static void visionox_rm692c9_panel_remove(struct mipi_dsi_device *dsi)
{
	struct visionox_rm692c9_panel *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id visionox_rm692c9_of_match[] = {
	{ .compatible = "visionox,rm692c9" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, visionox_rm692c9_of_match);

static struct mipi_dsi_driver visionox_rm692c9_panel_driver = {
    .driver =
        {
            .name = "panel-visionox-rm692c9",
            .of_match_table = visionox_rm692c9_of_match,
        },
    .probe = visionox_rm692c9_panel_probe,
    .remove = visionox_rm692c9_panel_remove,
};
module_mipi_dsi_driver(visionox_rm692c9_panel_driver);

MODULE_AUTHOR("Forairaaaaa");
MODULE_DESCRIPTION(
	"Visionox RM692C9 DSI Panel Driver (LG Wing 3.92 1080x1240 G-OLED)");
MODULE_LICENSE("GPL");