#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <sys/types.h>
#define l 1.0
#define k 0.3
#define c 1.0
#define a 0.0
#define b 1.0
#define min(x, y) (((x) < (y))? (x):(y))


typedef struct
{
   double *u;
   int *cur_time, start, end, *mode, Nt, N;
   pthread_barrier_t *bar;
}arguments;

int set_temp(double *u, unsigned int N) //считываем из файла первоначальное распределение температур
{
    FILE* f;
    unsigned int i, x;
    double temp;
    if((f = fopen("phi_tab", "r")) == NULL)
    {
        perror("open resource file");
        return 0;
    }
    for(i = 0; i < N; i++)
    {
       if ((!fscanf(f, "%d %lg", &x, &temp)) || (x >= N))
       {
           printf("Wrong number of parameters");
           return 1;
       }
       u[x] = temp;
    }
    fclose(f);
    return 1;
}

void *fill(void *args)
{
    arguments arg= *((arguments*) args);
    int prev_time = *arg.cur_time, i, ci, ai;
    while (*arg.cur_time != arg.Nt)
    {
        for (i = arg.start; i < arg.end; i++) //собственно, заполнение таблицы
        {
            ci = (*arg.mode) * arg.N + i; //current index -значение, которое хотим записать
            ai = (1 - *arg.mode) * arg.N + i; //above index -  значение "над" ним
            if (!i) //левая граница
                arg.u[ci] = a;
            else
            if (i == arg.N - 1) //правая граница
                arg.u[ci] = b;
            else
                arg.u[ci] = arg.u[ai] + k *(arg.u[ai + 1] - 2 * arg.u[ai] + arg.u[ai - 1]); //промежуточная точка
        }
        pthread_barrier_wait(arg.bar);
        while (prev_time == *arg.cur_time);
        prev_time = *arg.cur_time;
    }
    pthread_exit(NULL);
}

int main(int argc, char** argv)
{
    FILE *targ;
    double T, h, t, *u;
    int N, P, cur_time, mode, numsteps, Nt, step, i;
    pthread_t *threads;
    pthread_barrier_t bar;
    arguments *args;
    if ((argc == 4) && (sscanf(argv[1], "%d", &N)) && (sscanf(argv[2], "%lg", &T)) && (sscanf(argv[3], "%d", &P)))
    {
        h = l/N; //шаг по длине
        N++; //точек вывода = число отрезков + 1;
        t = k * h * h / (c * c); //шаг по времени
        Nt = (int)(T/t); //число шагов по времени
        numsteps = min(N, P); //если потоков больше, чем  "разрешение" сетки, уменьшаем количество работающих потоков
        step = ceil(N*1.0/numsteps); //число линий интегральной сетки, обрабатываемой одним потоком
        mode = 0;
        cur_time = 0;
        if ((threads = (pthread_t *) malloc ((P + 1) * sizeof(pthread_t))) != NULL)
        {
            if ((u = (double*) malloc(2 * N* sizeof(double*))) != NULL) //линейное представление массива 2*N: одна строка - стержень в предыдущий момент времен, вторая - в настоящий момент(чередуются)
            {
                if(set_temp(u, N))
                {
                    mode = 1;
                    if ((args = (arguments*) malloc(numsteps * sizeof(arguments))) != NULL) //массив аргументов для каждого потока
                    {
                        for(i = 0; i <numsteps; i++)
                        {
                            args[i].bar = &bar;
                            args[i].cur_time =&cur_time;
                            args[i].start = i *step; //начало участка
                            args[i].end = min((i + 1) * step, N); //конец участка
                            args[i].mode = &mode;
                            args[i].Nt = Nt;
                            args[i].N = N;
                            args[i].u = u;
                        }
                        pthread_barrier_init(&bar, NULL, numsteps + 1); //numsteps потоков + 1 для контроллирования процесса из main
                        cur_time = 0;
                        for (i = 0; i < numsteps; i++)
                            pthread_create(&threads[i + 1], NULL, &fill, args + i);
                        while (cur_time < Nt)
                        {
                            pthread_barrier_wait(&bar); //ждём пока текущая линия интегральной сетки будет высчитана
                            pthread_barrier_destroy(&bar); //обновляем баррьер
                            pthread_barrier_init(&bar, NULL, numsteps + 1);
                            mode = 1 - mode; //меняем заполняемую строчку массива
                            cur_time++; //переходим на следующую линию (по времени)
                        }
                        for (i = 0; i < P; i++)
                            pthread_join(threads[i], NULL);
                        if ((targ = fopen("RESULT", "w")) != NULL)
                        {
                            mode = 1 - mode;
                            for (i = 0; i < N; i++)
                                fprintf(targ, "%-8.5lg %lg\n", i*h, u[mode * N + i]);
                            fclose(targ);
                        }
                        else
                            perror("open target file");
                        free(args);
                    }
                    else
                        perror("args malloc");
                }
                free(u);
            }
            else
                perror("u malloc");
            free(threads);
        }
        else
            perror("threads malloc");
    }
    else
        puts("Wrong parameters!");
    return 0;
}
