// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/i2c.h>
#include <linux/acpi.h>
#include <linux/slab.h>

/*
 * Based on the Dacberry400 driver from OSA Electronics
 * Adapted for ES8311 by Rebecca
 */

static const struct snd_soc_dapm_widget rebecca_es8311_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_MIC("Mic", NULL),
	SND_SOC_DAPM_INPUT("MIC1"),
};

static const struct snd_soc_dapm_route rebecca_es8311_audio_map[] = {
	/* Speaker */
	{"Speaker", NULL, "OUT"},

	/* Mic */
	{"MIC1", NULL, "Mic"},
};

static int snd_rpi_rebecca_es8311_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = snd_soc_rtd_to_codec(rtd, 0);
	int ret;

	/*
	 * This board does not provide a master clock (MCLK).
	 * The ES8311 codec will be configured to derive its internal
	 * clock from the bit clock (BCLK).
	 * We signal this to the ASoC framework by setting the sysclk
	 * frequency to 0. The ES8311 codec driver should handle this.
	 */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, 0, 0);
	if (ret && ret != -ENOTSUPP) {
		dev_err(rtd->card->dev, "Failed to set codec sysclk: %d\n", ret);
		return ret;
	}

	return 0;
}

static int snd_rpi_rebecca_es8311_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	unsigned int bclk_ratio;

	/*
	 * The bcm2835-i2s driver is the master, so we need to set its
	 * BCLK ratio. A common setup is 32 bits per channel slot.
	 * The bclk_ratio is lrclk * channels * bits_per_slot.
	 * For stereo 32-bit slots, the ratio is 64.
	 */
	bclk_ratio = params_channels(params) * 32;

	return snd_soc_dai_set_bclk_ratio(cpu_dai, bclk_ratio);
}

static const struct snd_soc_ops snd_rpi_rebecca_es8311_ops = {
	.hw_params = snd_rpi_rebecca_es8311_hw_params,
};

SND_SOC_DAILINK_DEFS(rebecca_es8311,
	DAILINK_COMP_ARRAY(COMP_CPU("bcm2835-i2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("es8311.1-0018", "es8311-hifi")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("bcm2835-i2s.0")));

static struct snd_soc_dai_link snd_rpi_rebecca_es8311_dai[] = {
{
	.name = "ES8311",
	.stream_name = "ES8311 HiFi",
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		   SND_SOC_DAIFMT_CBS_CFS,
	.init = snd_rpi_rebecca_es8311_init,
	.ops = &snd_rpi_rebecca_es8311_ops,
	SND_SOC_DAILINK_REG(rebecca_es8311),
},
};

static struct snd_soc_card snd_rpi_rebecca_es8311 = {
	.owner = THIS_MODULE,
	.dai_link = snd_rpi_rebecca_es8311_dai,
	.num_links = ARRAY_SIZE(snd_rpi_rebecca_es8311_dai),
	.dapm_widgets = rebecca_es8311_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rebecca_es8311_widgets),
	.dapm_routes = rebecca_es8311_audio_map,
	.num_dapm_routes = ARRAY_SIZE(rebecca_es8311_audio_map),
};

static int snd_rpi_rebecca_es8311_probe(struct platform_device *pdev)
{
	int ret = 0;

	snd_rpi_rebecca_es8311.dev = &pdev->dev;

	if (pdev->dev.of_node) {
		struct device_node *i2s_node;
		struct snd_soc_card *card = &snd_rpi_rebecca_es8311;
		struct snd_soc_dai_link *dai = &snd_rpi_rebecca_es8311_dai[0];

		i2s_node = of_parse_phandle(pdev->dev.of_node,
						"i2s-controller", 0);
		if (i2s_node) {
			dai->cpus->dai_name = NULL;
			dai->cpus->of_node = i2s_node;
			dai->platforms->name = NULL;
			dai->platforms->of_node = i2s_node;
			of_node_put(i2s_node);
		}

		if (of_property_read_string(pdev->dev.of_node, "card-name",
						&card->name))
			card->name = "rebecca-es8311";
	}

	ret = snd_soc_register_card(&snd_rpi_rebecca_es8311);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"snd_soc_register_card() failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static void snd_rpi_rebecca_es8311_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&snd_rpi_rebecca_es8311);
}

static const struct of_device_id rebecca_es8311_match_id[] = {
	{ .compatible = "rebecca,audio", },
	{},
};
MODULE_DEVICE_TABLE(of, rebecca_es8311_match_id);

static struct platform_driver snd_rpi_rebecca_es8311_driver = {
	.driver = {
		.name = "snd-rpi-rebecca-es8311",
		.owner = THIS_MODULE,
		.of_match_table = rebecca_es8311_match_id,
	},
	.probe = snd_rpi_rebecca_es8311_probe,
	.remove = snd_rpi_rebecca_es8311_remove,
};

module_platform_driver(snd_rpi_rebecca_es8311_driver);

MODULE_AUTHOR("Forairaaaaa");
MODULE_DESCRIPTION("ES8311 sound card driver for Rebecca board");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rebecca-es8311");
MODULE_SOFTDEP("pre: snd-soc-es8311");
 