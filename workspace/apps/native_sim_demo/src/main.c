#include <stdint.h>
#include <zephyr/kernel.h>

#define PRODUCER_STACK_SIZE 1024
#define PRODUCER_PRIORITY 5
#define EVENT_INTERVAL K_MSEC(500)

K_MSGQ_DEFINE(event_queue, sizeof(uint32_t), 4, sizeof(uint32_t));

static void producer(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	uint32_t sequence = 1;

	while (true) {
		k_msgq_put(&event_queue, &sequence, K_FOREVER);
		sequence++;
		k_sleep(EVENT_INTERVAL);
	}
}

K_THREAD_DEFINE(producer_id, PRODUCER_STACK_SIZE, producer, NULL, NULL, NULL,
		PRODUCER_PRIORITY, 0, 0);

int main(void)
{
	uint32_t sequence;

	printk("native_sim demo started; press Ctrl+C to stop\n");

	while (true) {
		k_msgq_get(&event_queue, &sequence, K_FOREVER);
		printk("received event %u at %lld ms\n", sequence,
		       (long long)k_uptime_get());
	}

	return 0;
}
