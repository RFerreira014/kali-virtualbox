/*
**  mtms_final.c
**
**  e-Fólio B - Sistemas Operativos
**
**  Programa multitarefa em C99/POSIX que ordena uma sequência de inteiros
**  através do algoritmo Merge Sort, segundo a estrutura pedida no enunciado:
**
**    Ficheiro -> Leitora -> 2 Buffers de ordenação -> Ordenadoras
**             -> Buffer de fusão -> Fusão
**
**  Estrutura seguida nesta solução:
**  - a tarefa Leitora lê o ficheiro de entrada;
**  - existem 2 buffers fixos de ordenação;
**  - cada posição de buffer guarda o endereço de um subvetor com
**    no máximo 16 inteiros;
**  - quando a Leitora consegue aceder a um buffer livre, preenche-o
**    completamente;
**  - cada Ordenadora retira um único subvetor de cada vez;
**  - a tarefa de Fusão possui um vetor interno com dimensão n e vai
**    fundindo os subvetores ordenados até obter o resultado final.
**
**  O acesso aos buffers partilhados é protegido com mutex. Quando uma
**  tarefa não consegue avançar porque o recurso pretendido está ocupado,
**  faz espera ativa com sched_yield().
*/

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_N 20000
#define MAX_SUB 16
#define MAX_TRF 30
#define NBUF_ORD 2
#define POR_LINHA 12

typedef struct {
    int *ptr;
    int n;
    int ocupado;
} slot_t;

typedef struct {
    int id;
} ord_arg_t;

static char nome_ficheiro[1024];

static int v[MAX_N];
static int *resultado;

static int nt;
static int dim_ord;
static int dim_fus;
static int n_total;

static slot_t *buf_ord[NBUF_ORD];
static slot_t *buf_fus;

static int ord_ativo[NBUF_ORD];
static int ord_validos[NBUF_ORD];
static int ord_pendentes[NBUF_ORD];

static int subv_total;
static int subv_retirados;
static int res_n;

/* Contadores do relatório final pedido no enunciado. */
static int rel_leit_ints;
static int rel_leit_ok;
static int rel_leit_wait;
static int rel_ord_ints[MAX_TRF];
static int rel_ord_bufs[MAX_TRF];
static int rel_fus_ints;
static int rel_fus_nfus;

/*
** Sincronização mínima entre a tarefa principal e a tarefa Leitora:
** a Leitora lê primeiro o cabeçalho e só depois a tarefa principal
** cria os restantes recursos/tarefas.
*/
static volatile int cabecalho_lido;
static volatile int buffers_prontos;
static volatile int erro_leitura;

static pthread_mutex_t mtx_ord = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_fus = PTHREAD_MUTEX_INITIALIZER;

static void *tarefa_leitora(void *arg);
static void *tarefa_ordenadora(void *arg);
static void *tarefa_fusao(void *arg);
static void insertion_sort(int *a, int n);
static void merge_dois(int *base, int nb, const int *sub, int ns);
static void imprime_resultado(void);

int main(void)
{
    pthread_t tid_leit;
    pthread_t tid_fus;
    pthread_t tid_ord[MAX_TRF];
    pthread_attr_t attr;
    ord_arg_t args[MAX_TRF];
    int i;
    int r;

    /* Primeira string sem espaços recebida pela entrada padrão. */
    if (scanf("%1023s", nome_ficheiro) != 1) {
        fprintf(stderr, "falta o nome do ficheiro\n");
        return 1;
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    r = pthread_create(&tid_leit, &attr, tarefa_leitora, NULL);
    if (r != 0) {
        errno = r;
        perror("pthread_create leitora");
        return 1;
    }

    /* A tarefa principal espera que a Leitora leia o cabeçalho. */
    while (!cabecalho_lido && !erro_leitura) {
        sched_yield();
    }

    if (erro_leitura) {
        pthread_join(tid_leit, NULL);
        pthread_attr_destroy(&attr);
        return 1;
    }

    if (nt < 1 || nt > MAX_TRF) {
        fprintf(stderr, "nt inválido: %d\n", nt);
        return 1;
    }
    if (n_total < 0 || n_total > MAX_N) {
        fprintf(stderr, "n inválido: %d\n", n_total);
        return 1;
    }
    if (dim_ord < 1 || dim_fus < 1) {
        fprintf(stderr, "dimensão de buffer inválida\n");
        return 1;
    }

    resultado = malloc((size_t)n_total * sizeof(int));
    if (resultado == NULL) {
        perror("malloc");
        return 1;
    }

    for (i = 0; i < NBUF_ORD; i++) {
        buf_ord[i] = calloc((size_t)dim_ord, sizeof(slot_t));
        if (buf_ord[i] == NULL) {
            perror("calloc");
            return 1;
        }
    }

    buf_fus = calloc((size_t)dim_fus, sizeof(slot_t));
    if (buf_fus == NULL) {
        perror("calloc");
        return 1;
    }

    subv_total = (n_total + MAX_SUB - 1) / MAX_SUB;
    buffers_prontos = 1;

    for (i = 0; i < nt; i++) {
        args[i].id = i;
        r = pthread_create(&tid_ord[i], &attr, tarefa_ordenadora, &args[i]);
        if (r != 0) {
            errno = r;
            perror("pthread_create ordenadora");
            return 1;
        }
    }

    r = pthread_create(&tid_fus, &attr, tarefa_fusao, NULL);
    if (r != 0) {
        errno = r;
        perror("pthread_create fusão");
        return 1;
    }

    pthread_join(tid_leit, NULL);
    for (i = 0; i < nt; i++) {
        pthread_join(tid_ord[i], NULL);
    }
    pthread_join(tid_fus, NULL);

    pthread_attr_destroy(&attr);

    printf("nt=%d\n", nt);
    printf("n=%d\n", n_total);
    printf("T. Leitora:\n");
    printf("%d ints em %d+%d buffers\n", rel_leit_ints, rel_leit_ok, rel_leit_wait);
    printf("T. Ordenadoras(%d):\n", nt);
    for (i = 0; i < nt; i++) {
        printf("T_%d: %d ints em %d buffers\n", i, rel_ord_ints[i], rel_ord_bufs[i]);
    }
    printf("T. Fusão:\n");
    printf("%d ints em %d fusões\n", rel_fus_ints, rel_fus_nfus);
    printf("# dados ordenados\n");
    imprime_resultado();

    for (i = 0; i < NBUF_ORD; i++) {
        free(buf_ord[i]);
    }
    free(buf_fus);
    free(resultado);
    return 0;
}

static void *tarefa_leitora(void *arg)
{
    FILE *f;
    int pos = 0;

    (void)arg;

    f = fopen(nome_ficheiro, "r");
    if (f == NULL) {
        perror("fopen");
        erro_leitura = 1;
        return NULL;
    }

    /*
    ** A tarefa Leitora lê primeiro o cabeçalho, disponibiliza os
    ** parâmetros à tarefa principal e aguarda a criação dos buffers.
    */
    if (fscanf(f, " nt=%d", &nt) != 1 ||
        fscanf(f, " dim_buffer_ordenacao=%d", &dim_ord) != 1 ||
        fscanf(f, " dim_buffer_fusao=%d", &dim_fus) != 1 ||
        fscanf(f, " n=%d", &n_total) != 1) {
        fprintf(stderr, "erro no cabeçalho\n");
        fclose(f);
        erro_leitura = 1;
        cabecalho_lido = 1;
        return NULL;
    }

    {
        char linha[256];
        fgets(linha, sizeof(linha), f);
        fgets(linha, sizeof(linha), f);
    }

    cabecalho_lido = 1;

    while (!buffers_prontos) {
        sched_yield();
    }

    /* Só depois de os buffers estarem prontos é que lê todos os dados. */
    while (pos < n_total) {
        if (fscanf(f, "%d", &v[pos]) != 1) {
            fprintf(stderr, "erro a ler o dado %d\n", pos);
            fclose(f);
            erro_leitura = 1;
            return NULL;
        }
        pos++;
    }
    fclose(f);

    pos = 0;

    /*
    ** A Leitora escolhe aleatoriamente um dos dois buffers de ordenação.
    ** Se o buffer estiver ocupado, liberta o CPU e tenta novamente.
    ** Quando consegue aceder, preenche-o completamente.
    */
    while (pos < n_total) {
        int ib = rand() % NBUF_ORD;

        pthread_mutex_lock(&mtx_ord);
        if (!ord_ativo[ib]) {
            int i;
            int usados = 0;

            for (i = 0; i < dim_ord && pos < n_total; i++) {
                int tam = n_total - pos;
                int *sub;

                if (tam > MAX_SUB) {
                    tam = MAX_SUB;
                }

                sub = malloc((size_t)tam * sizeof(int));
                if (sub == NULL) {
                    perror("malloc leitora");
                    pthread_mutex_unlock(&mtx_ord);
                    erro_leitura = 1;
                    return NULL;
                }

                memcpy(sub, &v[pos], (size_t)tam * sizeof(int));
                buf_ord[ib][i].ptr = sub;
                buf_ord[ib][i].n = tam;
                buf_ord[ib][i].ocupado = 1;

                pos += tam;
                usados++;
                rel_leit_ints += tam;
                rel_leit_ok++;
            }

            for (i = usados; i < dim_ord; i++) {
                buf_ord[ib][i].ptr = NULL;
                buf_ord[ib][i].n = 0;
                buf_ord[ib][i].ocupado = 0;
            }

            ord_validos[ib] = usados;
            ord_pendentes[ib] = usados;
            ord_ativo[ib] = 1;
            pthread_mutex_unlock(&mtx_ord);
        } else {
            pthread_mutex_unlock(&mtx_ord);
            rel_leit_wait++;
            sched_yield();
        }
    }

    return NULL;
}

static void *tarefa_ordenadora(void *arg)
{
    ord_arg_t *a = (ord_arg_t *)arg;

    while (1) {
        int *sub = NULL;
        int nsub = 0;
        int ib = rand() % NBUF_ORD;

        /* A Ordenadora escolhe um buffer aleatoriamente e retira
        ** apenas um subvetor de cada vez. */
        pthread_mutex_lock(&mtx_ord);

        if (subv_retirados >= subv_total) {
            pthread_mutex_unlock(&mtx_ord);
            break;
        }

        if (ord_ativo[ib]) {
            int i;
            for (i = 0; i < ord_validos[ib]; i++) {
                if (buf_ord[ib][i].ocupado) {
                    sub = buf_ord[ib][i].ptr;
                    nsub = buf_ord[ib][i].n;
                    buf_ord[ib][i].ptr = NULL;
                    buf_ord[ib][i].n = 0;
                    buf_ord[ib][i].ocupado = 0;
                    ord_pendentes[ib]--;
                    subv_retirados++;
                    if (ord_pendentes[ib] == 0) {
                        ord_ativo[ib] = 0;
                        ord_validos[ib] = 0;
                    }
                    break;
                }
            }
        }

        pthread_mutex_unlock(&mtx_ord);

        if (sub == NULL) {
            sched_yield();
            continue;
        }

        insertion_sort(sub, nsub);
        rel_ord_ints[a->id] += nsub;
        rel_ord_bufs[a->id]++;

        /*
        ** Depois da ordenação local, o subvetor segue para o buffer
        ** de fusão, também com espera ativa se não houver posição livre.
        */
        while (1) {
            int i;
            int colocado = 0;

            pthread_mutex_lock(&mtx_fus);
            for (i = 0; i < dim_fus; i++) {
                if (!buf_fus[i].ocupado) {
                    buf_fus[i].ptr = sub;
                    buf_fus[i].n = nsub;
                    buf_fus[i].ocupado = 1;
                    colocado = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&mtx_fus);

            if (colocado) {
                break;
            }
            sched_yield();
        }
    }

    return NULL;
}

static void *tarefa_fusao(void *arg)
{
    (void)arg;

    /*
    ** A tarefa de Fusão possui um vetor interno com dimensão n.
    ** De cada vez que retira um subvetor do buffer de fusão,
    ** funde-o com o resultado parcial já acumulado.
    */
    while (res_n < n_total) {
        int *sub = NULL;
        int nsub = 0;
        int i;

        pthread_mutex_lock(&mtx_fus);
        for (i = 0; i < dim_fus; i++) {
            if (buf_fus[i].ocupado) {
                sub = buf_fus[i].ptr;
                nsub = buf_fus[i].n;
                buf_fus[i].ptr = NULL;
                buf_fus[i].n = 0;
                buf_fus[i].ocupado = 0;
                break;
            }
        }
        pthread_mutex_unlock(&mtx_fus);

        if (sub == NULL) {
            sched_yield();
            continue;
        }

        merge_dois(resultado, res_n, sub, nsub);
        res_n += nsub;
        rel_fus_ints += nsub;
        rel_fus_nfus++;
        free(sub);
    }

    return NULL;
}

static void insertion_sort(int *a, int n)
{
    int i;

    for (i = 1; i < n; i++) {
        int x = a[i];
        int j = i - 1;

        while (j >= 0 && a[j] > x) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = x;
    }
}

static void merge_dois(int *base, int nb, const int *sub, int ns)
{
    int i = 0;
    int j = 0;
    int k = 0;
    int *tmp;

    tmp = malloc((size_t)(nb + ns) * sizeof(int));
    if (tmp == NULL) {
        perror("malloc merge");
        exit(1);
    }

    while (i < nb && j < ns) {
        if (base[i] <= sub[j]) {
            tmp[k++] = base[i++];
        } else {
            tmp[k++] = sub[j++];
        }
    }
    while (i < nb) {
        tmp[k++] = base[i++];
    }
    while (j < ns) {
        tmp[k++] = sub[j++];
    }

    memcpy(base, tmp, (size_t)k * sizeof(int));
    free(tmp);
}

static void imprime_resultado(void)
{
    int i;
    int nlinhas = (res_n + POR_LINHA - 1) / POR_LINHA;

    if (nlinhas <= 10) {
        for (i = 0; i < res_n; i++) {
            if (i > 0 && i % POR_LINHA == 0) {
                printf("\n");
            }
            printf((i % POR_LINHA == 0) ? "%d" : " %d", resultado[i]);
        }
        printf("\n");
    } else {
        for (i = 0; i < POR_LINHA && i < res_n; i++) {
            printf((i == 0) ? "%d" : " %d", resultado[i]);
        }
        printf("\n...\n");
        {
            int ini = (nlinhas - 1) * POR_LINHA;
            for (i = ini; i < res_n; i++) {
                printf((i == ini) ? "%d" : " %d", resultado[i]);
            }
            printf("\n");
        }
    }
}
