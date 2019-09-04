#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>






#define PI 3.1415926535897932384626
#define NUM_TRAIN 792
#define NUM_TEST 193
#define RANGE 1.0
#define MIN_NUM_NB 5
#define MAX_NUM_TOT_EDGE 10000
#define MAX_ITER 2000000
#define TRAIN_RHO 0.0005 //step size for solving training problem
#define MU 0.1 //regularization parameter
#define TAULERANCE 0.000001
#define MASTER_COMP_DELAY 1.0
#define WORKER_COMP_DELAY_X 1.2
#define WORKER_COMP_DELAY_Z 0.6
#define MASTER_CYCLE 0.2
#define MAX_DELAY 6






double deg2rad (double deg){
    return ((deg/180.0)*PI);
}
double rad2deg (double rad){
    return ((rad/PI)*180.0);
}
double geo_distance (double lat1, double long1, double lat2, double long2){
    double theta, dist;
    theta = long1 - long2;
    dist = sin(deg2rad(lat1)) * sin(deg2rad(lat2)) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta));
    dist = acos(dist);
    dist = rad2deg(dist);
    dist = dist*60*1.1515; //unit is stature mile
    //dist = dist*1.609344; //unit is kilometer
    //dist = dist*0.8684; //unit is nautical mile
    return (dist);
}






int main (int argc, char *argv[]){
    //Input the csv file of training data:
    int i;
    int *train_house_id = (int *)malloc(sizeof(int)*NUM_TRAIN);
    double **train_data = (double **)malloc(sizeof(double *)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_data[i] = (double *)malloc(sizeof(double)*4);
        train_data[i][3] = 1.0;
    }
    double *train_label = (double *)malloc(sizeof(double)*NUM_TRAIN);
    double *train_latitude = (double *)malloc(sizeof(double)*NUM_TRAIN);
    double *train_longitude = (double *)malloc(sizeof(double)*NUM_TRAIN);
    char buf[1024];
    char temp[1024];

    FILE *fp1;
    char filename1[100] = "housing_price_prediction_train_data.csv";
    if(!(fp1 = fopen(filename1,"r"))){
        printf("Cannot open this file\n");
        exit(0);
    } 
    //skip header
    fgets(buf, 1024, fp1);
//printf("%s\n", buf);
    //read values
    i = 0;
    while (fgets(buf, 1024, fp1) != NULL){
//printf("%s\n", buf);
        if (!(sscanf(buf, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%lf,%lf,%d,%lf,%lf,%lf,%lf", temp, temp, temp, temp, temp, temp, temp, temp, temp, temp, &train_latitude[i], &train_longitude[i], &train_house_id[i], &train_data[i][0], &train_data[i][1], &train_data[i][2], &train_label[i]))){
            printf("Cannot scan this line\n");
            exit(0);
        }
//printf("%d\t%lf\t%lf\t%d\n", i, train_latitude[i], train_longitude[i], train_house_id[i]);
        if (++i >= NUM_TRAIN){
            break;
        }
    }
    fclose(fp1);




    //Construct the graph of training data:
    //calculate distances between each pair of training nodes:
    double **train_distance = (double **)malloc(sizeof(double *)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_distance[i] = (double *)malloc(sizeof(double)*NUM_TRAIN);
    }
    int j;
    for (i = 0; i < NUM_TRAIN; i++){
//printf("%d\n", i);
        for (j = 0; j < NUM_TRAIN; j++){
            train_distance[i][j] = geo_distance(train_latitude[i], train_longitude[i], train_latitude[j], train_longitude[j]);
        }
    }


    //output the distance matrix:
    //FILE *fp2;
    //char filename2[100] = "train_distance.csv";
    //if(!(fp2 = fopen(filename2,"w"))){
        //printf("Cannot open this file\n");
        //exit(0);
    //}
    //for (i = 0; i < NUM_TRAIN; i++){
        //for (j = 0; j < NUM_TRAIN - 1; j++){
            //fprintf(fp2, "%lf, ", train_distance[i][j]);
        //}
        //fprintf(fp2, "%lf\n", train_distance[i][NUM_TRAIN-1]);
    //}
    //fclose(fp2);

    //count the number of neighbors for each training node
    int *train_neighbor_count = (int *)malloc(sizeof(int)*NUM_TRAIN);   
    int train_neighbor_total;
    train_neighbor_total = 0;
    for (i = 0; i < NUM_TRAIN; i++){
        train_neighbor_count[i] = 0;
        for (j = 0; j < NUM_TRAIN; j++){
            if (j != i){
                if (train_distance[i][j] <= RANGE){
                    train_neighbor_count[i]++;
                    train_neighbor_total++;
                }
            }
        }
        if (train_neighbor_count[i] < MIN_NUM_NB){
            train_neighbor_total += -train_neighbor_count[i] + MIN_NUM_NB;
            train_neighbor_count[i] = MIN_NUM_NB;
        }
    }


    //select neighbors for each training node:
    int k, kk;
    double **train_weight = (double **)malloc(sizeof(double *)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_weight[i] = (double *)malloc(sizeof(double)*train_neighbor_count[i]);
        for (k = 0; k < train_neighbor_count[i]; k++){
            train_weight[i][k] = 1000000.0;
        }
    }
    int **train_neighbor_index = (int **)malloc(sizeof(int *)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_neighbor_index[i] = (int *)malloc(sizeof(int)*train_neighbor_count[i]);
        for (k = 0; k < train_neighbor_count[i]; k++){
            train_neighbor_index[i][k] = 1000000;
        }
    }
    for (i = 0; i < NUM_TRAIN; i++){
//printf("%d\t%d", i, train_neighbor_count[i]);
        for (j = 0; j < NUM_TRAIN; j++){
            if (j != i){
                for (k = 0; k < train_neighbor_count[i]; k++){
                    if (train_weight[i][k] > train_distance[i][j]){
                        break;
                    }
                }
                if (k == train_neighbor_count[i] - 1){
                    train_weight[i][k] = train_distance[i][j];
                    train_neighbor_index[i][k] = j;
                }
                else{
                    for (kk = train_neighbor_count[i]-2; kk > k-1; kk--){
                        train_weight[i][kk+1] = train_weight[i][kk];
                        train_neighbor_index[i][kk+1] = train_neighbor_index[i][kk];
                    }
                    train_weight[i][k] = train_distance[i][j];
                    train_neighbor_index[i][k] = j;
                }
            }
        }
        for (k = 0; k < train_neighbor_count[i]; k++){
//printf("\t%d", train_neighbor_index[i][k]);            
//printf("\t%lf", train_weight[i][k]);
            if (train_weight[i][k] < 0.001){
                train_weight[i][k] = 10000.0;
            }
            else{
                train_weight[i][k] = 1.0/train_weight[i][k];
            }
        }
//printf("\n");
    }


    //Construct edges:
    int train_num_edge;
    int **train_edge = (int **)malloc(sizeof(int *)*MAX_NUM_TOT_EDGE);
    for (i = 0; i < MAX_NUM_TOT_EDGE; i++){
        train_edge[i] = (int *)malloc(sizeof(int)*2);
    }
    double *train_edge_weight = (double *)malloc(sizeof(double)*MAX_NUM_TOT_EDGE);
    train_num_edge = 0;
    for (i = 0; i < NUM_TRAIN; i++){
//printf("%d: ", i);
        for (k = 0; k < train_neighbor_count[i]; k++){
            if (i < train_neighbor_index[i][k]){
                train_edge[train_num_edge][0] = i;
                train_edge[train_num_edge][1] = train_neighbor_index[i][k]; //edge "train_num_edge" connects node i and node "train_neighbor_index[i][k]"
                train_edge_weight[train_num_edge] = train_weight[i][k];
                train_num_edge++;
            }
            else{
                for (kk = 0; kk < train_num_edge; kk++){
                    if((train_edge[kk][0] == train_neighbor_index[i][k]) && (train_edge[kk][1] == i)){
                        break;
                    }
                }
                if (kk == train_num_edge){
                    train_edge[train_num_edge][0] = train_neighbor_index[i][k];
                    train_edge[train_num_edge][1] = i; //edge "train_num_edge" connects node i and node "train_neighbor_index[i][k]"
                    train_edge_weight[train_num_edge] = train_weight[i][k];
                    train_num_edge++;
                }
                else{
                    train_edge_weight[kk] += train_weight[i][k];
                }
            }
        }
//printf("%d\t%d\n", train_neighbor_count[i], train_copy_count[i]);
    }
printf("Total number of neighbors for training data is %d.\n", train_neighbor_total);
printf("Total number of edges for training data is %d.\n", train_num_edge);    




    //Input the csv file of testing data:
    int *test_house_id = (int *)malloc(sizeof(int)*NUM_TEST);
    double **test_data = (double **)malloc(sizeof(double *)*NUM_TEST);
    for (i = 0; i < NUM_TEST; i++){
        test_data[i] = (double *)malloc(sizeof(double)*4);
        test_data[i][3] = 1.0;
    }
    double *test_label = (double *)malloc(sizeof(double)*NUM_TEST);
    double *test_latitude = (double *)malloc(sizeof(double)*NUM_TEST);
    double *test_longitude = (double *)malloc(sizeof(double)*NUM_TEST);

    FILE *fp3;
    char filename3[100] = "housing_price_prediction_test_data.csv";
    if(!(fp3 = fopen(filename3,"r"))){
        printf("Cannot open this file\n");
        exit(0);
    } 
    //skip header
    fgets(buf, 1024, fp3);
//printf("%s\n", buf);
    //read values
    i = 0;
    while (fgets(buf, 1024, fp3) != NULL){
//printf("%s\n", buf);
        if (!(sscanf(buf, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%lf,%lf,%d,%lf,%lf,%lf,%lf", temp, temp, temp, temp, temp, temp, temp, temp, temp, temp, &test_latitude[i], &test_longitude[i], &test_house_id[i], &test_data[i][0], &test_data[i][1], &test_data[i][2], &test_label[i]))){
            printf("Cannot scan this line\n");
            exit(0);
        }
//printf("%d\t%lf\t%lf\t%d\n", i, test_latitude[i], test_longitude[i], test_house_id[i]);
        if (++i >= NUM_TEST){
            break;
        }
    }
    fclose(fp3);




    //Construct the graph of testing data:
    //calculate distances between each pair of testing node and training node:
    double **test_distance = (double **)malloc(sizeof(double *)*NUM_TEST);
    for (i = 0; i < NUM_TEST; i++){
        test_distance[i] = (double *)malloc(sizeof(double)*NUM_TRAIN);
    }
    for (i = 0; i < NUM_TEST; i++){
//printf("%d\n", i);
        for (j = 0; j < NUM_TRAIN; j++){
            test_distance[i][j] = geo_distance(test_latitude[i], test_longitude[i], train_latitude[j], train_longitude[j]);
        }
    }


    //output the distance matrix:
    //FILE *fp4;
    //char filename4[100] = "test_distance.csv";
    //if(!(fp4 = fopen(filename4,"w"))){
        //printf("Cannot open this file\n");
        //exit(0);
    //}
    //for (i = 0; i < NUM_TEST; i++){
        //for (j = 0; j < NUM_TRAIN - 1; j++){
            //fprintf(fp4, "%lf, ", test_distance[i][j]);
        //}
        //fprintf(fp4, "%lf\n", test_distance[i][NUM_TRAIN-1]);
    //}
    //fclose(fp4);

    //count the number of neighbors for each testing node:
    int *test_neighbor_count = (int *)malloc(sizeof(int)*NUM_TEST);
    int test_neighbor_total = 0;
    for (i = 0; i < NUM_TEST; i++){
//printf("%d: ", i);
        test_neighbor_count[i] = 0;
        for (j = 0; j < NUM_TRAIN; j++){
            if (test_distance[i][j] <= RANGE){
                test_neighbor_count[i]++;
                test_neighbor_total++;
            }
        }
        if (test_neighbor_count[i] < MIN_NUM_NB){
            test_neighbor_total += -test_neighbor_count[i] + MIN_NUM_NB;
            test_neighbor_count[i] = MIN_NUM_NB;
        }
//printf("%d\n", test_neighbor_count[i]);
    }
printf("Total number of neighbors for testing data is %d.\n", test_neighbor_total);

     
    //select neighbors for each testing node:
    double **test_weight = (double **)malloc(sizeof(double *)*NUM_TEST);
    for (i = 0; i < NUM_TEST; i++){
        test_weight[i] = (double *)malloc(sizeof(double)*test_neighbor_count[i]);
        for (k = 0; k < test_neighbor_count[i]; k++){
            test_weight[i][k] = 1000000.0;
        }
    }
    int **test_neighbor_index = (int **)malloc(sizeof(int *)*NUM_TEST);
    for (i = 0; i < NUM_TEST; i++){
        test_neighbor_index[i] = (int *)malloc(sizeof(int)*test_neighbor_count[i]);
        for (k = 0; k < test_neighbor_count[i]; k++){
            test_neighbor_index[i][k] = 1000000;
        }
    }
    for (i = 0; i < NUM_TEST; i++){
//printf("%d\t%d", i, test_neighbor_count[i]);
        for (j = 0; j < NUM_TRAIN; j++){
            for (k = 0; k < test_neighbor_count[i]; k++){
                if (test_weight[i][k] > test_distance[i][j]){
                    break;
                }
            }
            if (k == test_neighbor_count[i] - 1){
                test_weight[i][k] = test_distance[i][j];
                test_neighbor_index[i][k] = j;
            }
            else{
                for (kk = test_neighbor_count[i]-2; kk > k-1; kk--){
                    test_weight[i][kk+1] = test_weight[i][kk];
                    test_neighbor_index[i][kk+1] = test_neighbor_index[i][kk];
                }
                test_weight[i][k] = test_distance[i][j];
                test_neighbor_index[i][k] = j;
            }
        }
        for (k = 0; k < test_neighbor_count[i]; k++){
//printf("\t%d", test_neighbor_index[i][k]);
//printf("\t%lf", test_weight[i][k]);
            if (test_weight[i][k] < 0.001){
                test_weight[i][k] = 10000.0;
            }
            else{
                test_weight[i][k] = 1.0/test_weight[i][k];
            }
        }
//printf("\n");
    }
    


 
    //Open primal variables for training data on Master:
    //decision variables for nodes:
    double **train_x_M = (double **)malloc(sizeof(double *)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_x_M[i] = (double *)malloc(sizeof(double)*4);
    }
    //decision variables for edges:
    double **train_z_M = (double **)malloc(sizeof(double *)*train_num_edge);
    for (i = 0; i < train_num_edge; i++){
        train_z_M[i] = (double *)malloc(sizeof(double)*4);
    }
    //Open average primal variables for training data on Master:
    //decision variables for nodes:
    double **train_x_ave = (double **)malloc(sizeof(double *)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_x_ave[i] = (double *)malloc(sizeof(double)*4);
    }
    //decision variables for edges:
    double **train_z_ave = (double **)malloc(sizeof(double *)*train_num_edge);
    for (i = 0; i < train_num_edge; i++){
        train_z_ave[i] = (double *)malloc(sizeof(double)*4);
    }
    //Open dual variables for training data on Master:
    //correctors:
    double **train_lambda_M = (double **)malloc(sizeof(double *)*train_num_edge);
    for (i = 0; i < train_num_edge; i++){
        train_lambda_M[i] = (double *)malloc(sizeof(double)*4);
    }
    //predictors:
    double **train_p_M = (double **)malloc(sizeof(double *)*train_num_edge);
    for (i = 0; i < train_num_edge; i++){
        train_p_M[i] = (double *)malloc(sizeof(double)*4);
    }


    //Input more accurate solution for training data:
    double **train_x_accurate = (double **)malloc(sizeof(double *)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_x_accurate[i] = (double *)malloc(sizeof(double)*4);
    }
    FILE *fp5;
    char  filename5[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_More_Accurate_Solution";
    if(!(fp5 = fopen(filename5,"r"))){
        printf("Cannot open this file\n");
        exit(0);
    }
    for (i = 0; i < NUM_TRAIN; i++){
        fscanf(fp5, "%lf\t%lf\t%lf\t%lf", &train_x_accurate[i][0], &train_x_accurate[i][1], &train_x_accurate[i][2], &train_x_accurate[i][3]);
        
    }
    fclose(fp5);


    //Open asynchronous distribution parameters on Master:
    double *timeline_x = (double *)malloc(sizeof(double)*NUM_TRAIN);
    double *timeline_z = (double *)malloc(sizeof(double)*train_num_edge);
    double time_cut, time_min, time_max;
    int *arrival_x = (int *)malloc(sizeof(int)*NUM_TRAIN);
    int *arrival_z = (int *)malloc(sizeof(int)*train_num_edge);
    int *delay_x = (int *)malloc(sizeof(int)*NUM_TRAIN);
    int *delay_z = (int *)malloc(sizeof(int)*train_num_edge);
    int flag_one_more_delay;
    double arrival_rate_x, arrival_rate_z, arrival_rate;
    double *comm_delay_x = (double *)malloc(sizeof(double)*NUM_TRAIN);
    double *comm_delay_z = (double *)malloc(sizeof(double)*train_num_edge);
    //Output communication delays of each Worker:
    //FILE *fp5;
    //char filename5[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_Asynchronous_Communication_Delay";
    //if(!(fp5 = fopen(filename5, "w"))){
        //printf("Cannot open this file\n");
        //exit(0);
    //}
    //for (i = 0; i < NUM_TRAIN; i++){
        //comm_delay_x[i] = (double)(rand())/(double)(RAND_MAX);
        //fprintf(fp5, "%lf\n", comm_delay_x[i]);
    //}
    //for (i = 0; i < train_num_edge; i++){
        //comm_delay_z[i] = (double)(rand())/(double)(RAND_MAX);
        //fprintf(fp5, "%lf\n", comm_delay_z[i]);
    //}
    //fclose(fp5);

    //Input communication delays of each Worker:
    FILE *fp6;
    char filename6[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_Asynchronous_Communication_Delay";
    if(!(fp6 = fopen(filename6, "r"))){
        printf("Cannot open this file\n");
        exit(0);
    }
    for (i = 0; i < NUM_TRAIN; i++){
        fscanf(fp6, "%lf", &comm_delay_x[i]);
    }
    for (i = 0; i < train_num_edge; i++){
        fscanf(fp6, "%lf", &comm_delay_z[i]);
    }
    fclose(fp6);




    //Open primal variables for training data on Workers:
    //decision variables for nodes:
    double **train_x_W = (double **)malloc(sizeof(double *)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_x_W[i] = (double *)malloc(sizeof(double)*4);
    }
    //decision variables for edges:
    double **train_z_W = (double **)malloc(sizeof(double *)*train_num_edge);
    for (i = 0; i < train_num_edge; i++){
        train_z_W[i] = (double *)malloc(sizeof(double)*4);
    }
    //Open dual variables for training data on Workers:
    //predictors:
    double ***train_p_W_x = (double ***)malloc(sizeof(double **)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_p_W_x[i] = (double **)malloc(sizeof(double *)*train_num_edge);
        for (k = 0; k < train_num_edge; k++){
            train_p_W_x[i][k] = (double *)malloc(sizeof(double)*4);
        }
    }
    double **train_p_W_z = (double **)malloc(sizeof(double *)*train_num_edge);
    for (i = 0; i < train_num_edge; i++){
        train_p_W_z[i] = (double *)malloc(sizeof(double)*4);
    }


    //Open primal variables for testing data:
    //decision variables for nodes:
    double **test_x = (double **)malloc(sizeof(double *)*NUM_TEST);
    for (i = 0; i < NUM_TEST; i++){
        test_x[i] = (double *)malloc(sizeof(double)*4);
    }
    



    //N-block Predictor Corrector Proximal Multiplier Agorithm:
    int iter;
    double w; //overall penalty parameter
    double ratio;
    double regression, regression_ave;
    double train_objval, train_objval_ave;
    double train_residual, train_residual_ave;
    double error, error_ave;
    double ***matrix = (double ***)malloc(sizeof(double **)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        matrix[i] = (double **)malloc(sizeof(double *)*4);
        for (j = 0; j < 4; j++){
            matrix[i][j] = (double *)malloc(sizeof(double)*5);
        }
    }
    double temp1, temp2;
    double mean_square_error;
    

    w = 1.0;

    //Initiate primal variables for training data on Workers:
    //decision variables for nodes:
    for (i = 0; i < NUM_TRAIN; i++){
        for (j = 0; j < 4; j++){
            train_x_W[i][j] = 0.0; //initial value
        }
    }
    //decision variables for edges:
    for (i = 0; i < train_num_edge; i++){
        for (j = 0; j < 4; j++){
            train_z_W[i][j] = 0.0; //initial value
        }
    }
    //Initiate average primal variables for training data on Master:
    //decision variables for nodes:
    for (i = 0; i < NUM_TRAIN; i++){
        for (j = 0; j < 4; j++){
            train_x_ave[i][j] = 0.0; //initial value
        }
    }
    //decision variables for edges:
    for (i = 0; i < train_num_edge; i++){
        for (j = 0; j < 4; j++){
            train_z_ave[i][j] = 0.0; //initial value
        }
    }
    //Initiate dual variables for training data on Master:
    //correctors:
    for (i = 0; i < train_num_edge; i++){
        for (j = 0; j < 4; j++){
            train_lambda_M[i][j] = 0.0; //initial value
        }
    }
    //Initiate timelines on Master:
    for (i = 0; i < NUM_TRAIN; i++){
        timeline_x[i] = 0.0;
    } 
    for (i = 0; i < train_num_edge; i++){
        timeline_z[i] = 0.0;
    }
    //Initiate time cut on Master:
    time_cut = 0.0;
    //Initiate arrival signals on Master:
    for (i = 0; i < NUM_TRAIN; i++){
        arrival_x[i] = 1;
    }
    for (i = 0; i < train_num_edge; i++){
        arrival_z[i] = 1;
    }
    //Initiate delay counts on Master:
    for (i = 0; i < NUM_TRAIN; i++){
        delay_x[i] = 0;
    }
    for (i = 0; i < train_num_edge; i++){
        delay_z[i] = 0;
    }

    

    
    //Open file to output arrival rate of each iteration:
    FILE *fp7;
    char filename7[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_Asynchronous_Arrival_Rate";
    if(!(fp7 = fopen(filename7, "w"))){
        printf("Cannot open this file\n");
        exit(0);
    }


    //Open file to output arrival rate for nodes of each iteration:
    FILE *fp8;
    char filename8[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_Asynchronous_Arrival_Rate_x";
    if(!(fp8 = fopen(filename8, "w"))){
        printf("Cannot open this file\n");
        exit(0);
    }


    //Open file to output arrival rate for edges of each iteration:
    FILE *fp9;
    char filename9[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_Asynchronous_Arrival_Rate_z";
    if(!(fp9 = fopen(filename9, "w"))){
        printf("Cannot open this file\n");
        exit(0);
    }


    //Open file to output solution error of each iteration:
    FILE *fp10;
    char filename10[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_Asynchronous_Solution_Error";
    if(!(fp10 = fopen(filename10, "w"))){
        printf("Cannot open this file\n");
        exit(0);
    }


    //Open file to output average solution error of each iteration:
    FILE *fp11;
    char filename11[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_Asynchronous_Average_Solution_Error";
    if(!(fp11 = fopen(filename11, "w"))){
        printf("Cannot open this file\n");
        exit(0);
    }


    //Open file to output objective function value of each iteration:
    FILE *fp12;
    char filename12[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_Asynchronous_Objval";
    if(!(fp12 = fopen(filename12, "w"))){
        printf("Cannot open this file\n");
        exit(0);
    }
    

    //Open file to output average objective function value of each iteration:
    FILE *fp13;
    char filename13[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_Asynchronous_Average_Objval";
    if(!(fp13 = fopen(filename13, "w"))){
        printf("Cannot open this file\n");
        exit(0);
    }
    

    //Open file to output residual value of each iteration:
    FILE *fp14;
    char filename14[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_Asynchronous_Residual";
    if(!(fp14 = fopen(filename14, "w"))){
        printf("Cannot open this file\n");
        exit(0);
    }
       

    //Open file to output average residual value of each iteration:
    FILE *fp15;
    char filename15[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_Asynchronous_Average_Residual";
    if(!(fp15 = fopen(filename15, "w"))){
        printf("Cannot open this file\n");
        exit(0);
    }

    
    

    //Training Process:
    for (iter = 0; iter < MAX_ITER; iter++){
printf("%d th iteration:\t", iter);
        arrival_rate = 0.0;
        arrival_rate_x = 0.0;
        for (i = 0; i < NUM_TRAIN; i++){
            arrival_rate_x += (double)(arrival_x[i]);
        }
        arrival_rate += arrival_rate_x;
        arrival_rate_x = arrival_rate_x/NUM_TRAIN;
        fprintf(fp8, "%.16lf\n", arrival_rate_x);
        arrival_rate_z = 0.0;
        for (i = 0; i < train_num_edge; i++){
            arrival_rate_z += (double)(arrival_z[i]);
        }
        arrival_rate += arrival_rate_z;
        arrival_rate_z = arrival_rate_z/train_num_edge;
        fprintf(fp9, "%.16lf\n", arrival_rate_z);
        arrival_rate = arrival_rate/(NUM_TRAIN + train_num_edge);
        fprintf(fp7, "%.16lf\n", arrival_rate);
printf("%.2lf%%\t%.2lf%%\t%.2lf%%\t", arrival_rate*100, arrival_rate_x*100, arrival_rate_z*100);

        error = 0.0;
        error_ave = 0.0;

        train_objval = 0.0;
        train_objval_ave = 0.0;

        flag_one_more_delay = 1;


        //Update primal variables on Master for arrived Workers:
        //decision variables for nodes:
        for (i = 0; i < NUM_TRAIN; i++){            
            if (arrival_x[i]){
                regression = 0.0;
                regression_ave = 0.0;
                for (j = 0; j < 4; j++){
                    train_x_M[i][j] = train_x_W[i][j];
                    error += pow((double)(train_x_M[i][j] - train_x_accurate[i][j]), (double)2);
                    train_x_ave[i][j] = train_x_ave[i][j]*((double)(iter)/(double)(iter + 1)) + train_x_M[i][j]*(1.0/(double)(iter + 1));
                    error_ave += pow((double)(train_x_ave[i][j] - train_x_accurate[i][j]), (double)2);
                    regression += train_data[i][j]*train_x_M[i][j];
                    regression_ave += train_data[i][j]*train_x_ave[i][j];
                    if (j < 3){
                        train_objval += MU*pow((double)train_x_M[i][j], (double)2);
                        train_objval_ave += MU*pow((double)train_x_ave[i][j], (double)2);
                    }
                }
                train_objval += pow((double)(regression - train_label[i]), (double)2);
                train_objval_ave += pow((double)(regression_ave - train_label[i]), (double)2);
            }
            else{
                regression = 0.0;
                regression_ave = 0.0;
                for (j = 0; j < 4; j++){
                    error += pow((double)(train_x_M[i][j] - train_x_accurate[i][j]), (double)2);
                    train_x_ave[i][j] = train_x_ave[i][j]*((double)(iter)/(double)(iter + 1)) + train_x_M[i][j]*(1.0/(double)(iter + 1));
                    error_ave += pow((double)(train_x_ave[i][j] - train_x_accurate[i][j]), (double)2);
                    regression += train_data[i][j]*train_x_M[i][j];
                    regression_ave += train_data[i][j]*train_x_ave[i][j];
                    if (j < 3){
                        train_objval += MU*pow((double)train_x_M[i][j], (double)2);
                        train_objval_ave += MU*pow((double)train_x_ave[i][j], (double)2);
                    }
                }
                train_objval += pow((double)(regression - train_label[i]), (double)2);
                train_objval_ave += pow((double)(regression_ave - train_label[i]), (double)2);
            }
        }
        error = sqrt(error/NUM_TRAIN);
        fprintf(fp10, "%.16lf\n", error);
        error_ave = sqrt(error_ave/NUM_TRAIN);
        fprintf(fp11, "%.16lf\n", error_ave);

        //decision variables for edges:
        for (i = 0; i < train_num_edge; i++){
            if (arrival_z[i]){
                for (j = 0; j < 4; j++){
                    train_z_M[i][j] = train_z_W[i][j];
                    train_z_ave[i][j] = train_z_ave[i][j]*((double)(iter)/(double)(iter + 1)) + train_z_M[i][j]*(1.0/(double)(iter + 1));
                    train_objval += w*train_edge_weight[i]*pow((double)(train_z_M[i][j]), (double)2);
                    train_objval_ave += w*train_edge_weight[i]*pow((double)(train_z_ave[i][j]), (double)2);
                }
            }
            else{
                for (j = 0; j < 4; j++){
                    train_z_ave[i][j] = train_z_ave[i][j]*((double)(iter)/(double)(iter + 1)) + train_z_M[i][j]*(1.0/(double)(iter + 1));
                    train_objval += w*train_edge_weight[i]*pow((double)(train_z_M[i][j]), (double)2);
                    train_objval_ave += w*train_edge_weight[i]*pow((double)(train_z_ave[i][j]), (double)2);
                }
            }
        }
        fprintf(fp12, "%.16lf\n", train_objval);
        fprintf(fp13, "%.16lf\n", train_objval_ave);


        //Update dual correctors on Master:
        train_residual = 0.0;
        train_residual_ave = 0.0;
        for (i = 0; i < train_num_edge; i++){
            for (j = 0; j < 4; j++){
                train_lambda_M[i][j] += TRAIN_RHO*(train_x_M[train_edge[i][0]][j] - train_x_M[train_edge[i][1]][j] - train_z_M[i][j]);
                train_residual += pow((double)(train_x_M[train_edge[i][0]][j] - train_x_M[train_edge[i][1]][j] - train_z_M[i][j]), (double)2);
                train_residual_ave += pow((double)(train_x_ave[train_edge[i][0]][j] - train_x_ave[train_edge[i][1]][j] - train_z_ave[i][j]), (double)2);
            }
        }
        train_residual = sqrt(train_residual/train_num_edge);
        fprintf(fp14, "%.16lf\n", train_residual);
        train_residual_ave = sqrt(train_residual_ave/train_num_edge);
        fprintf(fp15, "%.16lf\n", train_residual_ave);
printf("%.12lf\t%.12lf\t%.12lf\t%.12lf\n", train_objval, train_objval_ave, train_residual, train_residual_ave);
        if (iter > MAX_DELAY && train_residual < TAULERANCE){
            break;
        }


        //Update delay counts on Master:
        for (i = 0; i < NUM_TRAIN; i++){
            if (arrival_x[i]){
                delay_x[i] = 0;
            }
            else{
                delay_x[i]++;
            }
            if (delay_x[i] == MAX_DELAY){
                flag_one_more_delay = 0;
            }
        }
        for (i = 0; i < train_num_edge; i++){
            if (arrival_z[i]){
                delay_z[i] = 0;
            }
            else{
                delay_z[i]++;
            }
            if (delay_z[i] == MAX_DELAY){
                flag_one_more_delay = 0;
            }
        }


        //Update dual predictors on Master:
        for (i = 0; i < train_num_edge; i++){
            for (j = 0; j < 4; j++){
                train_p_M[i][j] = train_lambda_M[i][j] + TRAIN_RHO*(train_x_M[train_edge[i][0]][j] - train_x_M[train_edge[i][1]][j] - train_z_M[i][j]);
            }
        }


        //Computation delay of Master:
        time_cut += MASTER_COMP_DELAY;


        //Broadcast dual predictors to arrived Workers:
        for (i = 0; i < NUM_TRAIN; i++){
            if (arrival_x[i]){
                for (k = 0; k < train_num_edge; k++){
                    for (j = 0; j < 4; j++){
                        train_p_W_x[i][k][j] = train_p_M[k][j];
                    }
                }
                //Update timeline with communication delay of Master to Worker:
                timeline_x[i] = time_cut + 1.5*comm_delay_x[i];
            }
        }
        for (i = 0; i < train_num_edge; i++){
            if (arrival_z[i]){
                for (j = 0; j < 4; j++){
                    train_p_W_z[i][j] = train_p_M[i][j];
                }
                //Update timeline with communication delay of Master to Worker:
                timeline_z[i] = time_cut + comm_delay_z[i];
            }
        }


        //Update primal variables on arrived Workers:
        //update decision variables for nodes:
        for (i = 0; i < NUM_TRAIN; i++){
            if (arrival_x[i]){
                //Gauss Elimination Method:
                //initiate matrix:
                for (j = 0; j < 4; j++){
                    if (j == 3){
                        for (k = 0; k < 4; k++){
                            if (k == j){
                                matrix[i][j][k] = 2*pow((double)train_data[i][3], (double)2) + 1/TRAIN_RHO;
                            }
                            else{
                                matrix[i][j][k] = 2*train_data[i][j]*train_data[i][k];
                            }
                        }
                    }
                    else{
                        for (k = 0; k < 4; k++){
                            if (k == j){
                                matrix[i][j][k] = 2*pow((double)train_data[i][j], (double)2) + 2*MU + 1/TRAIN_RHO;
                            }
                            else{
                                matrix[i][j][k] = 2*train_data[i][j]*train_data[i][k];
                            }
                        }
                    }
                    matrix[i][j][4] = 2*train_data[i][j]*train_label[i] + (1/TRAIN_RHO)*train_x_W[i][j];
                    for (kk = 0; kk < train_num_edge; kk++){
                        if (train_edge[kk][0] == i){
                            matrix[i][j][4] += -train_p_W_x[i][kk][j];
                        }
                        if (train_edge[kk][1] == i){
                            matrix[i][j][4] += train_p_W_x[i][kk][j];
                        }
                    }             
                }
                //calculate upper triangular matrix:
                for (k = 0; k < 4; k++){
                    for (j = 0; j < 4; j++){
                        if (j > k){
                            ratio = matrix[i][j][k]/matrix[i][k][k];
                            for (kk = 0; kk < 5; kk++){
                                matrix[i][j][kk] += -ratio*matrix[i][k][kk];
                            }
                        }
                    }
                }
                train_x_W[i][3] = matrix[i][3][4]/matrix[i][3][3];
                //backward substitution:
                for (j = 2; j >= 0; j--){
                    for (k = j+1; k < 4; k++){
                        matrix[i][j][4] += -matrix[i][j][k]*train_x_W[i][k];
                    }
                    train_x_W[i][j] = matrix[i][j][4]/matrix[i][j][j];
                }

                //Update timeline with computation delay:
                timeline_x[i] += WORKER_COMP_DELAY_X;
                //Update timeline with communication delay from Worker to Master:
                timeline_x[i] += comm_delay_x[i];
            }
        }

        //update decision variables for edges:
        for (i = 0; i < train_num_edge; i++){
            if (arrival_z[i]){
                for (j = 0; j < 4; j++){
                    train_z_W[i][j] = (train_p_W_z[i][j] + (1/TRAIN_RHO)*train_z_W[i][j])/(2*w*train_edge_weight[i] + (1/TRAIN_RHO));               
                }

                //Update timeline with computation delay:
                timeline_z[i] += WORKER_COMP_DELAY_Z;
                //Update timeline with communication delay from Worker to Master:
                timeline_z[i] += comm_delay_z[i];
            }
        }


        //Choose time cut for next iteration:
        if (flag_one_more_delay){
            time_min = timeline_x[0];
            for (i = 0; i < NUM_TRAIN; i++){
                if (timeline_x[i] < time_min){
                    time_min = timeline_x[i];
                }
            }
            for (i = 0; i < train_num_edge; i++){
                if (timeline_z[i] < time_min){
                    time_min = timeline_z[i];
                }
            }
            time_cut += ceil((time_min - time_cut)/MASTER_CYCLE)*MASTER_CYCLE;
        }
        else{
            time_max = 0.0;
            for (i = 0; i < NUM_TRAIN; i++){
                if (delay_x[i] == MAX_DELAY && timeline_x[i] > time_max){
                    time_max = timeline_x[i];
                }
            }
            for (i = 0; i < train_num_edge; i++){
                if (delay_z[i] == MAX_DELAY && timeline_z[i] > time_max){
                    time_max = timeline_z[i];
                }
            }
            time_cut += ceil((time_max - time_cut)/MASTER_CYCLE)*MASTER_CYCLE;
        }


        //Update arrival signals on Master:
        for (i = 0; i < NUM_TRAIN; i++){
            if (time_cut < timeline_x[i]){
                arrival_x[i] = 0;
            }
            else{
                arrival_x[i] = 1;
            }
        }
        for (i = 0; i < train_num_edge; i++){
            if (time_cut < timeline_z[i]){
                arrival_z[i] = 0;
            }
            else{
                arrival_z[i] = 1;
            }
        }
    }
    fclose(fp7);
    fclose(fp8);
    fclose(fp9);
    fclose(fp10);
    fclose(fp11);
    fclose(fp12);
    fclose(fp13);
    fclose(fp14); 
    fclose(fp15);




    //Testing Process:
    for (i = 0; i < NUM_TEST; i++){
        for (j = 0; j < 4; j++){
            temp1 = 0.0;
            temp2 = 0.0;
            for (k = 0; k < test_neighbor_count[i]; k++){
                temp1 += test_weight[i][k]*train_x_M[test_neighbor_index[i][k]][j];
                temp2 += test_weight[i][k];
            }
            test_x[i][j] = temp1/temp2;
        }
    }


    //Calculate MSE for testing data:
    mean_square_error = 0.0;
    for (i = 0; i < NUM_TEST; i++){
        regression = 0.0;
        for (j = 0; j < 4; j++){
            regression += test_data[i][j]*test_x[i][j];
        }
        mean_square_error += pow((double)(regression - test_label[i]), (double)2);
    }
    mean_square_error = mean_square_error/NUM_TEST;
printf("MSE of is %lf.\n", mean_square_error);




    //Output MSE and solutions for both training and testing data:
    FILE *fp16;
    char filename16[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_Asynchronous_Results";
    if(!(fp16 = fopen(filename16, "w"))){
        printf("Cannot open this file\n");
        exit(0);
    }
    fprintf(fp16, "MSE of is %lf.\n", mean_square_error);
    fprintf(fp16, "The solution for training data is:\n");
    for (i = 0; i < NUM_TRAIN; i++){
        fprintf(fp16, "%d\t%lf\t%lf\t%lf\t%lf\n", train_house_id[i], train_x_M[i][0], train_x_M[i][1], train_x_M[i][2], train_x_M[i][3]);
    }
    fprintf(fp16, "The solution for testing data is:\n");
    for (i = 0; i < NUM_TEST; i++){
        fprintf(fp16, "%d\t%lf\t%lf\t%lf\t%lf\n", test_house_id[i], test_x[i][0], test_x[i][1], test_x[i][2], test_x[i][3]);
    }
    fclose(fp16);
}