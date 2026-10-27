#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>

#include "a2_helper.h"

#define SEM_T25_FINISHED "/sem_t25_finished"
#define SEM_T31_FINISHED "/sem_t31_finished"

void process2();
void process3();
void process4();
void process5();
void process6();
void process7();
void process8();

void *thread_function_p2(void *thread_id);
void *thread_function_p3(void *thread_id);
void *thread_function_p4(void *thread_id);

sem_t *thread_25_finished;
sem_t *thread_31_finished;

sem_t thread_33_started;
sem_t thread_32_finished;

sem_t running_threads_limit;
sem_t thread_14_started_sem;
sem_t thread_14_can_end;
sem_t thread_14_finished_sem;

pthread_mutex_t p4_mutex;

int p4_running_threads = 0;
bool thread_14_started = false;
bool thread_14_finished = false;
bool thread_14_can_end_signal_sent = false;

void process6() {
    info(BEGIN, 6, 0);

    info(END, 6, 0);
}

void process7() {
    info(BEGIN, 7, 0);

    info(END, 7, 0);
}

void process8() {
    info(BEGIN, 8, 0);

    info(END, 8, 0);
}

void *thread_function_p2(void *thread_id) {
    int thread_no = *(int*)thread_id;

    if (thread_no == 4) {
        sem_wait(thread_31_finished);
    }

    info(BEGIN, 2, thread_no);

    info(END, 2, thread_no);

    if (thread_no == 5) {
        sem_post(thread_25_finished);
    }

    return NULL;
}

void process2() {
    info(BEGIN, 2, 0);

    sem_unlink(SEM_T25_FINISHED);
    sem_unlink(SEM_T31_FINISHED);

    thread_25_finished = sem_open(SEM_T25_FINISHED, O_CREAT, 0644, 0);
    thread_31_finished = sem_open(SEM_T31_FINISHED, O_CREAT, 0644, 0);

    if (thread_25_finished == SEM_FAILED || thread_31_finished == SEM_FAILED) {
        perror("sem_open fork failed");
        exit(1);
    }

    pid_t p3 = fork();

    if (p3 < 0) {
        perror("Process 3 fork failed");
        exit(1);
    }

    if (p3 == 0) {
        process3();
        exit(0);
    }

    pthread_t threads[5];
    int thread_ids[5] = {1, 2, 3, 4, 5};

    for (int i = 0; i < 5; i++) {
        pthread_create(&threads[i], NULL, thread_function_p2, &thread_ids[i]);
    }

    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }

    waitpid(p3, NULL, 0);

    sem_close(thread_25_finished);
    sem_close(thread_31_finished);

    sem_unlink(SEM_T25_FINISHED);
    sem_unlink(SEM_T31_FINISHED);

    info(END, 2, 0);
}

void *thread_function_p3(void *thread_id) {
    int thread_no = *(int*)thread_id;

    if (thread_no == 1) {
        sem_wait(thread_25_finished);
    }

    if (thread_no == 2) {
        sem_wait(&thread_33_started);
    }

    info(BEGIN, 3, thread_no);

    if (thread_no == 3) {
        sem_post(&thread_33_started);
        sem_wait(&thread_32_finished);
    }

    info(END, 3, thread_no);

    if (thread_no == 1) {
        sem_post(thread_31_finished);
    }

    if (thread_no == 2) {
        sem_post(&thread_32_finished);
    }

    return NULL;
}

void process3() {
    info(BEGIN, 3, 0);

    pid_t p4 = fork();

    if (p4 < 0) {
        perror("Process 4 fork failed");
        exit(1);
    }

    if (p4 == 0) {
        process4();
        exit(0);
    }

    pid_t p7 = fork();

    if (p7 < 0) {
        perror("Process 7 fork failed");
        exit(1);
    }

    if (p7 == 0) {
        process7();
        exit(0);
    }

    pid_t p5 = fork();

    if (p5 < 0) {
        perror("Process 5 fork failed");
        exit(1);
    }

    if (p5 == 0) {
        process5();
        exit(0);
    }

    pthread_t threads[4];
    int thread_ids[4] = {1, 2, 3, 4};

    sem_init(&thread_33_started, 0, 0);
    sem_init(&thread_32_finished, 0, 0);

    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, thread_function_p3, &thread_ids[i]);
    }

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&thread_33_started);
    sem_destroy(&thread_32_finished);

    waitpid(p4, NULL, 0);
    waitpid(p7, NULL, 0);
    waitpid(p5, NULL, 0);

    sem_close(thread_25_finished);
    sem_close(thread_31_finished);

    info(END, 3, 0);
}

void *thread_function_p4(void *thread_id) {
    int thread_no = *(int*)thread_id;

    sem_wait(&running_threads_limit);

    info(BEGIN, 4, thread_no);

    pthread_mutex_lock(&p4_mutex);

    p4_running_threads++;

    if (thread_no == 14) {
        thread_14_started = true;
        sem_post(&thread_14_started_sem);
    }

    if (thread_14_started == true &&
        thread_14_finished == false &&
        p4_running_threads == 4 &&
        thread_14_can_end_signal_sent == false) {

        thread_14_can_end_signal_sent = true;
        sem_post(&thread_14_can_end);
    }

    bool must_wait_for_14 = false;

    if (thread_no != 14 && thread_14_started == true && thread_14_finished == false) {
        must_wait_for_14 = true;
    }

    pthread_mutex_unlock(&p4_mutex);

    if (thread_no == 14) {
        sem_wait(&thread_14_can_end);

        info(END, 4, thread_no);

        pthread_mutex_lock(&p4_mutex);

        thread_14_finished = true;
        p4_running_threads--;

        pthread_mutex_unlock(&p4_mutex);

        for (int i = 0; i < 3; i++) {
            sem_post(&thread_14_finished_sem);
        }
    } else {
        if (must_wait_for_14 == true) {
            sem_wait(&thread_14_finished_sem);
        }

        info(END, 4, thread_no);

        pthread_mutex_lock(&p4_mutex);

        p4_running_threads--;

        pthread_mutex_unlock(&p4_mutex);
    }

    sem_post(&running_threads_limit);

    return NULL;
}

void process4() {
    info(BEGIN, 4, 0);

    pthread_t threads[50];
    int thread_ids[50];

    for (int i = 0; i < 50; i++) {
        thread_ids[i] = i + 1;
    }

    sem_init(&running_threads_limit, 0, 4);
    sem_init(&thread_14_started_sem, 0, 0);
    sem_init(&thread_14_can_end, 0, 0);
    sem_init(&thread_14_finished_sem, 0, 0);

    pthread_mutex_init(&p4_mutex, NULL);

    p4_running_threads = 0;
    thread_14_started = false;
    thread_14_finished = false;
    thread_14_can_end_signal_sent = false;

    pthread_create(&threads[13], NULL, thread_function_p4, &thread_ids[13]);

    sem_wait(&thread_14_started_sem);

    for (int i = 0; i < 50; i++) {
        if (i != 13) {
            pthread_create(&threads[i], NULL, thread_function_p4, &thread_ids[i]);
        }
    }

    for (int i = 0; i < 50; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&p4_mutex);

    sem_destroy(&running_threads_limit);
    sem_destroy(&thread_14_started_sem);
    sem_destroy(&thread_14_can_end);
    sem_destroy(&thread_14_finished_sem);

    info(END, 4, 0);
}

void process5() {
    info(BEGIN, 5, 0);

    pid_t p8 = fork();

    if (p8 < 0) {
        perror("Process 8 fork failed");
        exit(1);
    }

    if (p8 == 0) {
        process8();
        exit(0);
    }

    waitpid(p8, NULL, 0);

    info(END, 5, 0);
}

int main() {
    init();

    info(BEGIN, 1, 0);

    pid_t p2 = fork();

    if (p2 < 0) {
        perror("Process 2 fork failed");
        exit(1);
    }

    if (p2 == 0) {
        process2();
        exit(0);
    }

    pid_t p6 = fork();

    if (p6 < 0) {
        perror("Process 6 fork failed");
        exit(1);
    }

    if (p6 == 0) {
        process6();
        exit(0);
    }

    waitpid(p2, NULL, 0);
    waitpid(p6, NULL, 0);

    info(END, 1, 0);

    return 0;
}