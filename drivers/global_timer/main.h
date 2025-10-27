/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __GLB_TIMER_MAIN_H_
#define __GLB_TIMER_MAIN_H_

#define init_func(f) meson_glb_timer_##f##_driver_init
#define exit_func(f) meson_glb_timer_##f##_driver_exit

#define glb_module_init(f)	init_func(f)()
#define glb_module_exit(f)	exit_func(f)()

#define module_driver_only_def(__driver, __register, __unregister, ...) \
static typeof(__driver) *___driver = &(__driver);			\
void __init __driver##_init(void)					\
{									\
	if (__register(___driver, ##__VA_ARGS__) < 0)			\
		pr_err("%s:probe failed\n", __func__);			\
}									\
void __exit __driver##_exit(void)					\
{									\
	__unregister(___driver, ##__VA_ARGS__);				\
}

#define module_platform_driver_only_define(__platform_driver)			\
	module_driver_only_def(__platform_driver, platform_driver_register,	\
			       platform_driver_unregister)

#if IS_ENABLED(CONFIG_AMLOGIC_GLOBAL_TIMER_CORE)
void __init meson_glb_timer_core_driver_init(void);
void __exit meson_glb_timer_core_driver_exit(void);
#else
static inline void __init meson_glb_timer_core_driver_init(void){}
static inline void __exit meson_glb_timer_core_driver_exit(void){}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_GLOBAL_TIMER_ISP)
void __init meson_glb_timer_isp_driver_init(void);
void __exit meson_glb_timer_isp_driver_exit(void);
#else
static inline void __init meson_glb_timer_isp_driver_init(void){}
static inline void __exit meson_glb_timer_isp_driver_exit(void){}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_GLOBAL_TIMER_OUTPUT)
void __init meson_glb_timer_output_driver_init(void);
void __exit meson_glb_timer_output_driver_exit(void);
#else
static inline void __init meson_glb_timer_output_driver_init(void){}
static inline void __exit meson_glb_timer_output_driver_exit(void){}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_GLOBAL_TIMER_INPUT)
void __init meson_glb_timer_input_driver_init(void);
void __exit meson_glb_timer_input_driver_exit(void);
#else
static inline void __init meson_glb_timer_input_driver_init(void){}
static inline void __exit meson_glb_timer_input_driver_exit(void){}
#endif

#endif /*__GPIO_MAIN_H_*/
