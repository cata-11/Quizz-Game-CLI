struct
{
    sqlite3 *data;
} db;

struct
{
    char data[4096];
    int count;
    int timer;
    int correct_answers[1024];
    int number_of_questions;
} quizz;

int check_sql(int err, const char *msg)
{
    if (err != SQLITE_OK)
        printf("Error: %s (%d)\n", msg, err), exit(0);
    return err;
}

int sql_callback(void *data, int argc, char **argv, char **col)
{
    int i;

    char temp[4096];
    char extracted_data[4096];

    strcpy(extracted_data, "\0");
    strcpy(quizz.data, "\0");

    for (int i = 0; i < argc; i++)
    {
        sprintf(temp, "%s: %s", col[i], argv[i]);
        strcat(extracted_data, temp);
        strcat(extracted_data, "\n");
    }
    strcat(extracted_data, "\0");
    strcat(quizz.data, extracted_data);

    if (!strcmp(col[0], "TIME"))
        quizz.timer = atoi(&quizz.data[6]);

    return 0;
}

int sql_callbak_count(void *data, int argc, char **argv, char **col)
{
    char temp[2];
    strcpy(temp, argv[0]);
    quizz.count = atoi(&temp[0]);
    return 0;
}

int x = 0;
int sql_callback_correct_answer(void *data, int argc, char **argv, char **col)
{

    for (int i = 0; i < argc; i++)
    {
        if (i == 0)
            x++;
        if (i == 1)
            quizz.correct_answers[x] = atoi(argv[i]);
    }
    return 0;
}

void extractQuestion(int id)
{
    char sql_select_question[256];

    sprintf(sql_select_question, "SELECT QUESTION FROM QUIZZ WHERE ID = %d;", id);

    check_sql(sqlite3_open("quizz.db", &db.data),
              "Failed to open database");
    check_sql(sqlite3_exec(db.data, sql_select_question, sql_callback, NULL, NULL),
              "Failed to extract question from data base");
    check_sql(sqlite3_close(db.data),
              "Failed to close data base");
}

void extractAnswers(int id)
{
    char sql_select_answers[256];

    sprintf(sql_select_answers, "SELECT V1, V2, V3, V4, V5 FROM QUIZZ WHERE ID = %d;", id);

    check_sql(sqlite3_open("quizz.db", &db.data),
              "Failed to open database");
    check_sql(sqlite3_exec(db.data, sql_select_answers, sql_callback, NULL, NULL),
              "Failed to extract answers from data base");
    check_sql(sqlite3_close(db.data),
              "Failed to close data base");
}

void extractTimer(int id)
{
    char sql_select_time[256];

    sprintf(sql_select_time, "SELECT TIME FROM QUIZZ WHERE ID = %d;", id);

    check_sql(sqlite3_open("quizz.db", &db.data),
              "Failed to open database");
    check_sql(sqlite3_exec(db.data, sql_select_time, sql_callback, NULL, NULL),
              "Failed to extract answers from data base");
    check_sql(sqlite3_close(db.data),
              "Failed to close data base");
}

void extractCorrectAnswers()
{
    char sql_select_answer[256];

    sprintf(sql_select_answer, "SELECT ID, ANSWER FROM ANSWERS;");

    check_sql(sqlite3_open("quizz.db", &db.data),
              "Failed to open database");
    check_sql(sqlite3_exec(db.data, sql_select_answer, sql_callback_correct_answer, NULL, NULL),
              "Failed to extract correct answers from data base");
    check_sql(sqlite3_close(db.data),
              "Failed to close data base");
}

void createDatabase()
{
    char *err;
    char *sql_quizz_table = "DROP TABLE IF EXISTS QUIZZ;"
                            "CREATE TABLE QUIZZ(ID INT PRIMARY KEY, QUESTION TEXT, V1 TEXT, V2 TEXT, V3 TEXT, V4 TEXT, V5 TEXT, TIME INT);"
                            "INSERT INTO QUIZZ VALUES(1, 'Q1', 'Q1_1','Q1_2','Q1_3','Q1_4', 'Q1_5', 5);"
                            "INSERT INTO QUIZZ VALUES(2, 'Q2', 'Q2_1','Q2_2','Q2_3','Q2_4', 'Q2_5', 5);"
                            "INSERT INTO QUIZZ VALUES(3, 'Q3', 'Q3_1','Q3_2','Q3_3','Q3_4', 'Q3_5', 5);"
                            "INSERT INTO QUIZZ VALUES(4, 'Q4', 'Q4_1','Q4_2','Q4_3','Q4_4', 'Q4_5', 5);"
                            "INSERT INTO QUIZZ VALUES(5, 'Q5', 'Q5_1','Q5_2','Q5_3','Q5_4', 'Q5_5', 5);";

    char *sql_answers_table = "DROP TABLE IF EXISTS ANSWERS;"
                              "CREATE TABLE ANSWERS(ID INT PRIMARY KEY, ANSWER INT);"
                              "INSERT INTO ANSWERS VALUES(1, 1);"
                              "INSERT INTO ANSWERS VALUES(2, 2);"
                              "INSERT INTO ANSWERS VALUES(3, 3);"
                              "INSERT INTO ANSWERS VALUES(4, 4);"
                              "INSERT INTO ANSWERS VALUES(5, 5);";

    char *sql_cmd_quizz_count = "SELECT COUNT(*) FROM QUIZZ";

    check_sql(sqlite3_open("quizz.db", &db.data),
              "Failed to open database");
    check_sql(sqlite3_exec(db.data, sql_quizz_table, NULL, 0, &err),
              "Failed to create table");
    check_sql(sqlite3_exec(db.data, sql_cmd_quizz_count, sql_callbak_count, 0, &err),
              "Failed to count questions");
    check_sql(sqlite3_exec(db.data, sql_answers_table, NULL, 0, &err),
              "Failed to create table");
    check_sql(sqlite3_close(db.data),
              "Failed to close data base");
}
