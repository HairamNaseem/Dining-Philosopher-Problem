#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/semaphore.h>
#include <linux/time.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HNR");
MODULE_DESCRIPTION("Dining Philosophers System Call");

#define N 5
#define THINKING 0
#define HUNGRY 1
#define EATING 2
#define MAX_WAIT 3
#define RUNTIME_SECONDS 60

static DEFINE_MUTEX(mutex);
static int state[N];
static int wait_counts[N];
static struct task_struct *philosophers[N];
static struct semaphore forks[N];
static time64_t start_time;

static int left(int philosopher) {
    return (philosopher + N - 1) % N;
}

static int right(int philosopher) {
    return (philosopher + 1) % N;
}

static void check(int philosopher) {
    if (state[philosopher] == HUNGRY && state[left(philosopher)] != EATING &&
        state[right(philosopher)] != EATING && wait_counts[philosopher] < MAX_WAIT) {
        state[philosopher] = EATING;
        printk(KERN_INFO "Philosopher %d acquired forks %d and %d\n", philosopher, left(philosopher), philosopher);
        printk(KERN_INFO "Philosopher %d is eating\n", philosopher);
    }
}

static void take_forks(int philosopher) {
    down_interruptible(&forks[philosopher]);
    state[philosopher] = HUNGRY;
    check(philosopher);
    up(&forks[philosopher]);
}

static void put_forks(int philosopher) {
    state[philosopher] = THINKING;
    printk(KERN_INFO "Philosopher %d finished eating, now thinking\n", philosopher);
    wait_counts[left(philosopher)] = 0;
    wait_counts[right(philosopher)] = 0;
    check(left(philosopher));
    check(right(philosopher));
}

static int philosopher_thread(void *arg) {
    int philosopher = *(int *)arg;

    while (ktime_get_seconds() < start_time + RUNTIME_SECONDS) {
        printk(KERN_INFO "Philosopher %d is thinking\n", philosopher);
        msleep_interruptible(get_random_int() % 5000 + 1000);

        take_forks(philosopher);
        wait_counts[philosopher]++;
        msleep_interruptible(get_random_int() % 5000 + 1000);
        put_forks(philosopher);
    }

    return 0;
}

asmlinkage long sys_dining(void) {
    int i;

    for (i = 0; i < N; i++) {
        sem_init(&forks[i], 1);
        state[i] = THINKING;
        wait_counts[i] = 0;
    }

    start_time = ktime_get_seconds();

    for (i = 0; i < N; i++) {
        int *philosopher = kmalloc(sizeof(int), GFP_KERNEL);
        *philosopher = i;
        philosophers[i] = kthread_run(philosopher_thread, philosopher, "philosopher%d", i);
        msleep(1000);
    }

    for (i = 0; i < N; i++) {
        kthread_stop(philosophers[i]);
    }

    for (i = 0; i < N; i++) {
        sem_destroy(&forks[i]);
    }

    return 0;
}

