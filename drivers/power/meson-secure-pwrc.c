// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <dt-bindings/power/amlogic-a9-power.h>
#include <linux/amlogic/power_domain.h>
#include <linux/arm-smccc.h>
#include <linux/amlogic/meson_sm.h>
#include <linux/module.h>

#define PWRC_ON		1
#define PWRC_OFF	0
#define PWRC_NO_PARENT	UINT_MAX

struct meson_secure_pwrc_domain {
	struct generic_pm_domain base;
	unsigned int index;
	unsigned int parent;
	struct meson_secure_pwrc *pwrc;
};

struct meson_secure_pwrc {
	struct meson_secure_pwrc_domain *domains;
	struct genpd_onecell_data xlate;
	struct meson_sm_firmware *fw;
};

struct meson_secure_pwrc_domain_desc {
	unsigned int index;
	unsigned int parent;
	unsigned int flags;
	char *name;
	bool (*is_off)(struct meson_secure_pwrc_domain *pwrc_domain);
};

struct meson_secure_pwrc_domain_data {
	unsigned int count;
	const struct meson_secure_pwrc_domain_desc *domains;
};

static bool pwrc_secure_is_off(struct meson_secure_pwrc_domain *pwrc_domain)
{
	int is_off = 1;

	if (meson_sm_call(pwrc_domain->pwrc->fw, SM_A1_PWRC_GET, &is_off,
			  pwrc_domain->index, 0, 0, 0, 0) < 0)
		pr_err("failed to get power domain status\n");

	return is_off;
}

static int meson_secure_pwrc_off(struct generic_pm_domain *domain)
{
	int ret = 0;
	struct meson_secure_pwrc_domain *pwrc_domain =
		container_of(domain, struct meson_secure_pwrc_domain, base);

	if (meson_sm_call(pwrc_domain->pwrc->fw, SM_A1_PWRC_SET, NULL,
			  pwrc_domain->index, PWRC_OFF, 0, 0, 0) < 0) {
		pr_err("failed to set power domain off\n");
		ret = -EINVAL;
	}

	return ret;
}

static int meson_secure_pwrc_on(struct generic_pm_domain *domain)
{
	int ret = 0;
	struct meson_secure_pwrc_domain *pwrc_domain =
		container_of(domain, struct meson_secure_pwrc_domain, base);

	if (meson_sm_call(pwrc_domain->pwrc->fw, SM_A1_PWRC_SET, NULL,
			  pwrc_domain->index, PWRC_ON, 0, 0, 0) < 0) {
		pr_err("failed to set power domain on\n");
		ret = -EINVAL;
	}

	return ret;
}

#define SEC_PD(__name, __flag)			\
[PWRC_##__name##_ID] =				\
{						\
	.name = #__name,			\
	.index = PWRC_##__name##_ID,		\
	.is_off = pwrc_secure_is_off,		\
	.flags = __flag,			\
	.parent = PWRC_NO_PARENT,		\
}

#define TOP_PD(__name, __flag, __parent)	\
[PWRC_##__name##_ID] =				\
{						\
	.name = #__name,			\
	.index = PWRC_##__name##_ID,		\
	.is_off = pwrc_secure_is_off,		\
	.flags = __flag,			\
	.parent = __parent,			\
}

static const struct meson_secure_pwrc_domain_desc a9_pwrc_domains[] = {
	SEC_PD(A9_DSPA,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_U3HSG_U3,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_DP,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_DOS_HCODEC,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_TAHOE,		GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_DOS_HEVC,		GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_U2H,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_U3DRD_B,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_VPU_HDMI,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_U2DRD,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_U3DRD_A,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_SD_EMMC_C,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_GE2D,		GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_AMFC,		GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_EDPTX,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_OPP,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_VICP,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_SD_EMMC_A,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_SD_EMMC_B,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_ETH,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_PCIE_A,		GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_PCIE_B,		GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_NNA_4T,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_HDMIRX,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_CVE,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_ISP,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_ETH_1G,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_U3HSG_HSG,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_U3DPPHY_U3,		GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_U3DPPHY_DP,		GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_PCIE3PHY,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_U3HSG_PCIE2,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_MALI_TOP,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_AO_SED,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_AO_IR,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_AO_UART_B,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_AO_UART_C,		GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_AO_UART_D,		GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_AO_SPISG,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_AO_UART_E,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_AO_CEC,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_EE_SRAMA,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_AUDIO,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_DMC0,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_GIC,		GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_DDRPHY,		GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_AUCPU,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_DSI0,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_DSI1,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_CAN0,	GENPD_FLAG_ALWAYS_ON),
	SEC_PD(A9_CAN1,	GENPD_FLAG_ALWAYS_ON),
};

static int meson_secure_pwrc_probe(struct platform_device *pdev)
{
	int i, ret;
	struct device_node *sm_np;
	struct meson_secure_pwrc *pwrc;
	const struct meson_secure_pwrc_domain_data *match;

	match = of_device_get_match_data(&pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "failed to get match data\n");
		return -ENODEV;
	}

	sm_np = of_find_compatible_node(NULL, NULL, "amlogic,meson-gxbb-sm");
	if (!sm_np) {
		dev_err(&pdev->dev, "no secure-monitor node\n");
		return -ENODEV;
	}

	pwrc = devm_kzalloc(&pdev->dev, sizeof(*pwrc), GFP_KERNEL);
	if (!pwrc) {
		of_node_put(sm_np);
		return -ENOMEM;
	}

	pwrc->fw = meson_sm_get(sm_np);
	of_node_put(sm_np);
	if (!pwrc->fw)
		return -EPROBE_DEFER;

	pwrc->xlate.domains = devm_kcalloc(&pdev->dev, match->count,
					   sizeof(*pwrc->xlate.domains),
					   GFP_KERNEL);
	if (!pwrc->xlate.domains)
		return -ENOMEM;

	pwrc->domains = devm_kcalloc(&pdev->dev, match->count,
				     sizeof(*pwrc->domains), GFP_KERNEL);
	if (!pwrc->domains)
		return -ENOMEM;

	pwrc->xlate.num_domains = match->count;
	platform_set_drvdata(pdev, pwrc);

	for (i = 0 ; i < match->count ; ++i) {
		struct meson_secure_pwrc_domain *dom = &pwrc->domains[i];

		if (!match->domains[i].name)
			continue;

		dom->pwrc = pwrc;
		dom->index = match->domains[i].index;
		dom->parent = match->domains[i].parent;
		dom->base.name = match->domains[i].name;
		dom->base.flags = match->domains[i].flags;
		dom->base.power_on = meson_secure_pwrc_on;
		dom->base.power_off = meson_secure_pwrc_off;

		if (match->domains[i].is_off(dom) && (dom->base.flags & GENPD_FLAG_ALWAYS_ON))
			meson_secure_pwrc_on(&dom->base);

		ret = pm_genpd_init(&dom->base, NULL, match->domains[i].is_off(dom));
		if (ret) {
			dev_err(&pdev->dev, "failed to init domain %s\n", dom->base.name);
			return ret;
		}

		pwrc->xlate.domains[i] = &dom->base;
	}

	for (i = 0; i < match->count; i++) {
		struct meson_secure_pwrc_domain *dom = pwrc->domains;

		if (!match->domains[i].name || match->domains[i].parent == PWRC_NO_PARENT)
			continue;

		ret = pm_genpd_add_subdomain(&dom[dom[i].parent].base, &dom[i].base);
		if (ret) {
			dev_err(&pdev->dev, "failed to add %s subdomain to parent %s\n",
				dom[i].base.name, dom[dom[i].parent].base.name);
			return ret;
		}
	}

	pd_dev_create_file(&pdev->dev, 0, pwrc->xlate.num_domains,
			   pwrc->xlate.domains);

	pr_info("power domain probe done\n");

	return of_genpd_add_provider_onecell(pdev->dev.of_node, &pwrc->xlate);
}

static struct meson_secure_pwrc_domain_data amlogic_secure_a9_pwrc_data = {
	.domains = a9_pwrc_domains,
	.count = ARRAY_SIZE(a9_pwrc_domains),
};

static const struct of_device_id meson_secure_pwrc_match_table[] = {
	{
		.compatible = "amlogic,a9-pwrc",
		.data = &amlogic_secure_a9_pwrc_data,
	},
	{ /* sentinel */ }
};

static struct platform_driver meson_secure_pwrc_driver = {
	.probe = meson_secure_pwrc_probe,
	.driver = {
		.name		= "meson_secure_pwrc",
		.of_match_table	= meson_secure_pwrc_match_table,
	},
};

int __init meson_power_domain_init(void)
{
	return platform_driver_register(&meson_secure_pwrc_driver);
}

void __exit meson_power_domain_exit(void)
{
	platform_driver_unregister(&meson_secure_pwrc_driver);
}
