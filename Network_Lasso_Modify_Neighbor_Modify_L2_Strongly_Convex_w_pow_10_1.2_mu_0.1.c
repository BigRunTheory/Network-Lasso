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
#define MAX_ITER 100000
#define TRAIN_RHO 0.17 //step size for solving training problem
#define MU 0.1 //regularization parameter
#define TAULERANCE 0.000001






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
    //char  filename4[100] = "test_distance.csv";
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
    


 
    //Open primal variables for training data:
    //decision variables for nodes:
    double **train_x = (double **)malloc(sizeof(double *)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_x[i] = (double *)malloc(sizeof(double)*4);
    }
    //decision variables for edges:
    double **train_z = (double **)malloc(sizeof(double *)*train_num_edge);
    for (i = 0; i < train_num_edge; i++){
        train_z[i] = (double *)malloc(sizeof(double)*4);
    }
    //Open dual variables for training data on Master:
    //correctors:
    double **train_lambda = (double **)malloc(sizeof(double *)*train_num_edge);
    for (i = 0; i < train_num_edge; i++){
        train_lambda[i] = (double *)malloc(sizeof(double)*4);
    }
    //predictors:
    double **train_p = (double **)malloc(sizeof(double *)*train_num_edge);
    for (i = 0; i < train_num_edge; i++){
        train_p[i] = (double *)malloc(sizeof(double)*4);
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
    double regression;
    double train_objval;
    double train_residual;
    double ***matrix = (double ***)malloc(sizeof(double **)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        matrix[i] = (double **)malloc(sizeof(double *)*4);
        for (j = 0; j < 4; j++){
            matrix[i][j] = (double *)malloc(sizeof(double)*5);
        }
    }
    double temp1, temp2;
    double mean_square_error;
    
    
    w = pow((double)10.0, (double)1.2);

    //Initiate primal variables for training data:
    //decision variables for nodes:
    for (i = 0; i < NUM_TRAIN; i++){
        for (j = 0; j < 4; j++){
            train_x[i][j] = 0.0; //initial value
        }
    }
    //decision variables for edges:
    for (i = 0; i < train_num_edge; i++){
        for (j = 0; j < 4; j++){
            train_z[i][j] = 0.0; //initial value
        }
    }
    //Initiate dual variables for training data:
    //correctors:
    for (i = 0; i < train_num_edge; i++){
        for (j = 0; j < 4; j++){
            train_lambda[i][j] = 0.0; //initial value
        }
    }
    
    


    //Training Process:
    for (iter = 0; iter < MAX_ITER; iter++){
printf("%d th iteration:\t", iter);
        train_objval = 0.0;


        //Update dual predictors on Master:
        for (i = 0; i < train_num_edge; i++){
            for (j = 0; j < 4; j++){
                train_p[i][j] = train_lambda[i][j] + TRAIN_RHO*(train_x[train_edge[i][0]][j] - train_x[train_edge[i][1]][j] - train_z[i][j]);
            }
        }


        //Update primal variables:
        //update decision variables for nodes:
        for (i = 0; i < NUM_TRAIN; i++){
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
                matrix[i][j][4] = 2*train_data[i][j]*train_label[i] + (1/TRAIN_RHO)*train_x[i][j];
                for (kk = 0; kk < train_num_edge; kk++){
                    if (train_edge[kk][0] == i){
                        matrix[i][j][4] += -train_p[kk][j];
                    }
                    if (train_edge[kk][1] == i){
                        matrix[i][j][4] += train_p[kk][j];
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
            train_x[i][3] = matrix[i][3][4]/matrix[i][3][3];
            //backward substitution:
            for (j = 2; j >= 0; j--){
                for (k = j+1; k < 4; k++){
                    matrix[i][j][4] += -matrix[i][j][k]*train_x[i][k];
                }
                train_x[i][j] = matrix[i][j][4]/matrix[i][j][j];
            }
            //calculate objective function value:
            regression = 0.0;
            for (j = 0; j < 4; j++){
                regression += train_data[i][j]*train_x[i][j];
                if (j < 3){
                    train_objval += MU*pow((double)train_x[i][j], (double)2);
                }
            }
            train_objval += pow((double)(regression - train_label[i]), (double)2);
        }
        
        //update decision variables for edges:
        for (i = 0; i < train_num_edge; i++){
            for (j = 0; j < 4; j++){
                train_z[i][j] = (train_p[i][j] + (1/TRAIN_RHO)*train_z[i][j])/(2*w*train_edge_weight[i] + (1/TRAIN_RHO));
                train_objval += w*train_edge_weight[i]*pow((double)(train_z[i][j]), (double)2);
            }
        }

            
        //Update dual correctors:
        train_residual = 0.0;
        for (i = 0; i < train_num_edge; i++){
            for (j = 0; j < 4; j++){
                train_lambda[i][j] += TRAIN_RHO*(train_x[train_edge[i][0]][j] - train_x[train_edge[i][1]][j] - train_z[i][j]);
                train_residual += pow((double)(train_x[train_edge[i][0]][j] - train_x[train_edge[i][1]][j] - train_z[i][j]), (double)2);
            }
        }
        train_residual = sqrt(train_residual/train_num_edge);
printf("%lf\t%.12lf\n", train_objval, train_residual);
        if (train_residual < TAULERANCE){
            break;
        }
    }




    //Testing Process:
    for (i = 0; i < NUM_TEST; i++){
        for (j = 0; j < 4; j++){
            temp1 = 0.0;
            temp2 = 0.0;
            for (k = 0; k < test_neighbor_count[i]; k++){
                temp1 += test_weight[i][k]*train_x[test_neighbor_index[i][k]][j];
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
    



    //Output MSE:
    FILE *fp5;
    char filename5[100] = "Network_Lasso_Modify_Neighbor_Modify_L2_Strongly_Convex_w_pow_10_1.2_mu_0.1_MSE";
    if(!(fp5 = fopen(filename5, "w"))){
        printf("Cannot open this file\n");
        exit(0);
    }
    fprintf(fp5, "MSE of is %.16lf.\n", mean_square_error);
    fclose(fp5);
}