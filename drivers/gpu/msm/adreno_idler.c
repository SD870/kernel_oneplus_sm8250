/*
 * Adreno Idler for Qualcomm MSM GPUs
 * Original author: arter97
 * * Optimized to reduce power consumption by actively
 * dropping the GPU frequency to its lowest state
 * when the workload is minimal.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include "kgsl_device.h"
#include "kgsl_pwrctrl.h"

static unsigned int adreno_idler_idlewait = 20;
static unsigned int adreno_idler_idleworkload = 5000;
static unsigned int adreno_idler_downdifferential = 20;

module_param_named(idlewait, adreno_idler_idlewait, uint, 0644);
module_param_named(idleworkload, adreno_idler_idleworkload, uint, 0644);
module_param_named(downdifferential, adreno_idler_downdifferential, uint, 0644);

static bool adreno_idler_active = false;
static struct delayed_work adreno_idler_work;
static struct kgsl_device *adreno_idler_device;

static void adreno_idler_work_func(struct work_struct *work)
{
	struct kgsl_pwrctrl *pwr = &adreno_idler_device->pwrctrl;
	struct kgsl_pwrscale *pwrscale = &pwr->pwrscale;
	unsigned int active_pwrlevel = pwr->active_pwrlevel;
	unsigned int default_pwrlevel = pwr->default_pwrlevel;
	unsigned int min_pwrlevel = pwr->min_pwrlevel;
	unsigned int max_pwrlevel = pwr->max_pwrlevel;
	int idle_time, busy_time;

	if (!adreno_idler_active || !pwrscale->enabled)
		goto out;

	if (active_pwrlevel == min_pwrlevel)
		goto out;

	/* Calculate GPU workload */
	kgsl_pwrscale_update_stats(adreno_idler_device);
	idle_time = pwrscale->time_profile.idle_time;
	busy_time = pwrscale->time_profile.busy_time;

	/* If GPU is practically doing nothing */
	if (busy_time < adreno_idler_idleworkload) {
		/* Drop it to the absolute minimum frequency */
		kgsl_pwrctrl_pwrlevel_change(adreno_idler_device, min_pwrlevel);
	}

out:
	queue_delayed_work(system_unbound_wq, &adreno_idler_work,
			   msecs_to_jiffies(adreno_idler_idlewait));
}

void adreno_idler_init(struct kgsl_device *device)
{
	if (!device)
		return;

	adreno_idler_device = device;
	adreno_idler_active = true;

	INIT_DELAYED_WORK(&adreno_idler_work, adreno_idler_work_func);
	queue_delayed_work(system_unbound_wq, &adreno_idler_work,
			   msecs_to_jiffies(adreno_idler_idlewait));
}
EXPORT_SYMBOL(adreno_idler_init);

void adreno_idler_exit(void)
{
	adreno_idler_active = false;
	cancel_delayed_work_sync(&adreno_idler_work);
}
EXPORT_SYMBOL(adreno_idler_exit);
