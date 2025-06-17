// SPDX-License-Identifier: GPL-2.0
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct lg3p92_panel {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
    uint8_t prepared;
};

static inline struct lg3p92_panel *to_lg3p92_panel(struct drm_panel *panel)
{
	return container_of(panel, struct lg3p92_panel, panel);
}

static void lg3p92_panel_reset(struct lg3p92_panel *ctx)
{
    // TODO
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(25);
}

static void lg3p92_panel_on(struct mipi_dsi_multi_context *dsi_ctx)
{
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xFE, 0x00);
    mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xC2, 0x08);
    mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0x35, 0x00);

    mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0x51, 0x07, 0xFF);
    mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0x11);  // exit sleep
    mipi_dsi_msleep(dsi_ctx, 120);

    mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0x29);  // display on
    mipi_dsi_msleep(dsi_ctx, 80);
}

static void lg3p92_panel_off(struct mipi_dsi_multi_context *dsi_ctx)
{
    // TODO
    mipi_dsi_dcs_set_display_off_multi(dsi_ctx);

	mipi_dsi_msleep(dsi_ctx, 60);

	mipi_dsi_dcs_enter_sleep_mode_multi(dsi_ctx);

    mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0x28);
    mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0x10);
}

static int lg3p92_panel_prepare(struct drm_panel *panel)
{
	struct lg3p92_panel *ctx = to_lg3p92_panel(panel);
    struct mipi_dsi_multi_context dsi_ctx =	{.dsi = ctx->dsi};

	if (ctx->prepared)
		return 0;

	lg3p92_panel_reset(ctx);

	lg3p92_panel_on(&dsi_ctx);

	ctx->prepared = true;
	return 0;
}

static int lg3p92_panel_unprepare(struct drm_panel *panel)
{
	struct lg3p92_panel *ctx = to_lg3p92_panel(panel);
    struct mipi_dsi_multi_context dsi_ctx =	{.dsi = ctx->dsi};

	if (!ctx->prepared)
		return 0;

	lg3p92_panel_off(&dsi_ctx);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode panel_mode = {
	.clock = 88000, // kHz
	.hdisplay = 1080,
	.hsync_start = 1080 + 40,
	.hsync_end = 1080 + 40 + 4,
	.htotal = 1080 + 40 + 4 + 40,

	.vdisplay = 1240,
	.vsync_start = 1240 + 16,
	.vsync_end = 1240 + 16 + 4,
	.vtotal = 1240 + 16 + 4 + 20,

	.width_mm = 70,
	.height_mm = 80,

    .type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static int lg3p92_panel_get_modes(struct drm_panel *panel,
			     struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &panel_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	drm_mode_probed_add(connector, mode);
	return 1;
}

static const struct drm_panel_funcs lg3p92_panel_funcs = {
	.prepare = lg3p92_panel_prepare,
	.unprepare = lg3p92_panel_unprepare,
	.get_modes = lg3p92_panel_get_modes,
};

static int lg3p92_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lg3p92_panel *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio), "Failed to get reset GPIO\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
	                  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET;

	drm_panel_init(&ctx->panel, dev, &lg3p92_panel_funcs, DRM_MODE_CONNECTOR_DSI);
	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void lg3p92_panel_remove(struct mipi_dsi_device *dsi)
{
	struct lg3p92_panel *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id lg3p92_of_match[] = {
	{ .compatible = "lg,lg3p92" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lg3p92_of_match);

static struct mipi_dsi_driver lg3p92_panel_driver = {
	.driver = {
		.name = "panel-lg3p92",
		.of_match_table = lg3p92_of_match,
	},
	.probe = lg3p92_panel_probe,
	.remove = lg3p92_panel_remove,
};
module_mipi_dsi_driver(lg3p92_panel_driver);

MODULE_AUTHOR("Dashabi");
MODULE_DESCRIPTION("LG 3.92 AMOLED panel driver");
MODULE_LICENSE("GPL");
