#include <iostream>
#include <opencv2/opencv.hpp>
#include <fstream>
#include <string>
#include <random>
#include <vector>
#include <cmath>

using namespace std;

vector <vector<int>> ret_idx(){
    vector<vector<int>> IDX = {
        {3, 4}, {2, 5}, {1, 6}, {0, 7},
        {1, 7}, {2, 6}, {3, 5}, {4, 4},
        {5, 3}, {6, 2}, {7, 1}, {7, 2},
        {6, 3}, {5, 4}, {4, 5}, {3, 6},
        {2, 7}, {3, 7}, {4, 6}, {5, 5},
        {6, 4}, {7, 3}, {7, 4}, {6, 5},
        {5, 6}, {4, 7}, {5, 7}, {6, 6},
        {7, 5}, {7, 6}, {6, 7}, {7, 7}
    };
    return IDX;
}

int sign(int x){
    return (x > 0) - (x < 0);
}

vector <int> generate_blocks(int size){
    vector <int> permutation;
    for (int i = 0; i < size; i++) {
        permutation.push_back(i);
    }   
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(permutation.begin(), permutation.end(), gen);
    return permutation;
}

std::vector<std::vector<double>> do_dct(const std::vector<std::vector<int>>& input) {
    cv::Mat inputMat(input.size(), input[0].size(), CV_32S);
    for (size_t i = 0; i < input.size(); i++) {
        for (size_t j = 0; j < input[0].size(); j++) {
            inputMat.at<int>(i, j) = input[i][j];
        }
    }

    // Convert input matrix to CV_32F (float) for DCT computation
    cv::Mat floatInput;
    inputMat.convertTo(floatInput, CV_32F);

    // Perform 2D DCT without normalization
    cv::Mat dctResult;
    cv::dct(floatInput, dctResult);

    // Manual normalization (equivalent to 'norm="ortho"' in SciPy)

    std::vector<std::vector<double>> output(dctResult.rows, std::vector<double>(dctResult.cols));
    for (int i = 0; i < dctResult.rows; i++) {
        for (int j = 0; j < dctResult.cols; j++) {
            output[i][j] = dctResult.at<float>(i, j);
        }
    }

    return output;
}

// Function to compute the 2D IDCT on DCT coefficients to recover the original data
std::vector<std::vector<int>> undo_dct(const std::vector<std::vector<double>>& dctCoefficients) {
    cv::Mat dctResult(dctCoefficients.size(), dctCoefficients[0].size(), CV_64FC1);
    for (size_t i = 0; i < dctCoefficients.size(); i++) {
        for (size_t j = 0; j < dctCoefficients[0].size(); j++) {
            dctResult.at<double>(i, j) = dctCoefficients[i][j];
        }
    }

    // Perform 2D IDCT without normalization
    cv::Mat idctResult;
    cv::idct(dctResult, idctResult);

    // Convert back to integer data
    std::vector<std::vector<int>> output(idctResult.rows, std::vector<int>(idctResult.cols));
    for (int i = 0; i < idctResult.rows; i++) {
        for (int j = 0; j < idctResult.cols; j++) {
            output[i][j] = static_cast<int>(idctResult.at<double>(i, j));
        }
    }

    return output;
}

std::vector<std::vector<double>> embed_to_dct(std::vector<std::vector<double>> dct_matrix, const string bit_string, double q = 8.0){
    int ind = 0;
    std::vector<std::vector<int>> IDX = ret_idx();
    for (const auto& coord : IDX) {
        int i = coord[0], j = coord[1];
        dct_matrix[i][j] = sign(dct_matrix[i][j]) * (q * int(abs(dct_matrix[i][j]) / q) + (q/2) * (int(bit_string[ind]) - int('0')));
        ind += 1;
    }
    return dct_matrix;
}

std::vector<std::vector<double>> generate_population(vector<vector<int>> original,vector<vector<int>> notorig, int population_size = 128,double beta = 0.9,int search_space = 10){
    std::random_device rd;
    std::mt19937 gen(rd());
    double lower_bound = 0.0;
    double upper_bound = 1.0;
    std::uniform_real_distribution<double> uniform_dist(lower_bound, upper_bound);
    std::uniform_int_distribution<int> search_dist(-search_space, search_space);

    vector<double> diff; //flatten
    for (int i = 0; i < original.size(); i++)
        for (int j = 0; j < original[0].size();j++)
            diff.push_back(original[i][j]-notorig[i][j]);
    
    vector<vector<double>> population(population_size-1,vector<double>(diff.size()));
    population.push_back(diff);

    for (int i = 0; i < population_size; i++){
        for (int j = 0; j < diff.size(); j++){
            double random_value = uniform_dist(gen);
            if (random_value > beta){
                int random_search = search_dist(gen); 
                population[i][j] = random_search;
            }
            else{
                population[i][j] = diff[j];
            }
        }
    }
    return population;
}

std::vector<double> meanAlongAxis(const std::vector<std::vector<double>>& array) {
    std::vector<double> meanValues(array[0].size(), 0.0);

    for (int j = 0; j < array[0].size(); j++) {
        double sum = 0.0;
        for (int i = 0; i < array.size(); i++) {
            sum += array[i][j];
        }
        meanValues[j] = sum / array.size();
    }

    return meanValues;
}

double getRandomInteger(int search_space) {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> distribution(-search_space, search_space);
    int randomInteger = distribution(rng);
    return double(randomInteger);
}

class Metric{
    private:
    std::vector<std::vector<int>> block_matrix;
    std::string bit_string;
    int search_space;
    public: 
    Metric(const std::vector<std::vector<int>>& block_matrix, const std::string& bit_string, const int& search_space)
        : block_matrix(block_matrix), bit_string(bit_string), search_space(search_space) {}

    std::pair<double, vector<double>> metric(const std::vector<double>& block, int q = 8) {//block float?

        std::vector<vector<int>> new_block = block_matrix;
        vector<double> block_flatten = block; 
        for (int i = 0; i < block_flatten.size(); i++){
            block_flatten[i] = floor(block_flatten[i]);
            if ((block_flatten[i] < -search_space) || (block_flatten[i] > search_space))
               block_flatten[i] = getRandomInteger(search_space);
        }

        int ind_fl = 0;
        for (int i = 0; i < 8; i++){
            for (int j = 0; j < 8; j++){
                new_block[i][j] += block_flatten[ind_fl];
                if (new_block[i][j] > 255){
                    int diff = abs(new_block[i][j]-255);
                    block_flatten[ind_fl] -= diff;
                    new_block[i][j] = 255;
                }
                if (new_block[i][j] < 0){
                    int diff = abs(new_block[i][j]);
                    block_flatten[ind_fl] += diff;
                    new_block[i][j] = 0;
                }
                ind_fl++;
            }
        }
        int sum_elem = 0;
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
                sum_elem += pow(block_matrix[i][j] - new_block[i][j],2);
        double psnr = 10 * log10((pow(8,2) * pow(255,2)) / double(sum_elem));
    
        //extraction
        vector <vector<double>> dct_block = do_dct(new_block);
        string s;
        vector<vector<int>> IDX = ret_idx();
        for (const auto& coord : IDX) {
            int i = coord[0], j = coord[1];
            int c0 = sign(dct_block[i][j]) * (q * int(abs(dct_block[i][j]) / q) + (q/2) * (0));
            int c1 = sign(dct_block[i][j]) * (q * int(abs(dct_block[i][j]) / q) + (q/2) * (1));
            if (abs(dct_block[i][j] - c0) < abs(dct_block[i][j] - c1))
                s += '0';
            else
                s += '1';
            if (s[0] != bit_string[0]){
                std::pair<double, vector<double>> to_ret = std::make_pair(0.0, block_flatten);
                return to_ret;
            }
        }
        int cnt = 0;
        for (int i = 0; i < s.length(); i++)
            if (s[i] == bit_string[i])
                cnt += 1;
        //cout << s << endl;
        std::pair<double, vector<double>> to_ret = std::make_pair(psnr/10000 + double(cnt)/double(s.length()), block_flatten);
        return to_ret;
    }
};


std::vector<double> getRandomArray(size_t size, double low, double high) {
    std::vector<double> randomArray;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> distribution(low, high);

    for (size_t i = 0; i < size; i++) {
        randomArray.push_back(distribution(gen));
    }

    return randomArray;
}

std::vector<double> calculateDifference(const std::vector<double>& teacher, const std::vector<double>& population_mean) {
    size_t num_features = teacher.size();
    std::vector<double> difference;
    
    std::vector<double> randomUniform1 = getRandomArray(num_features, 0.0, 1.0);
    std::vector<double> randomUniform2 = getRandomArray(num_features, 1.0, 2.0);
    
    for (size_t i = 0; i < num_features; i++) {
        difference.push_back(randomUniform1[i] * (teacher[i] - randomUniform2[i] * population_mean[i]));
    }

    return difference;
}

std::vector<double> calculateDifferenceRand(const std::vector<double>& rand1,const std::vector<double>& rand2, const std::vector<double>& population_cur) {
    size_t num_features = population_cur.size();
    std::vector<double> new_population;
    
    std::vector<double> randomUniform1 = getRandomArray(num_features, 0.0, 1.0);
    
    for (size_t i = 0; i < num_features; i++) {
        new_population.push_back(population_cur[i] + randomUniform1[i] * (rand1[i] - rand2[i]));
    }

    return new_population;
}

std::vector<double> calculateDifferenceSCA(const std::vector<double>& random_agent,const std::vector<double>& agent, const double C) {
    size_t num_features = agent.size();
    std::vector<double> D;
        
    for (size_t i = 0; i < num_features; i++) {
        D.push_back(abs(C * random_agent[i] - agent[i]));
    }

    return D;
}

std::vector<double> calculateDifferenceRandomPositonSCA(const std::vector<double>& random_agent,const std::vector<double>& D, const double A) {
    size_t num_features = D.size();
    std::vector<double> new_position;
        
    for (size_t i = 0; i < num_features; i++) {
        new_position.push_back(random_agent[i] - D[i]*A);
    }

    return new_position;
}

int getRandomIndex(int population_size) {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<int> uniform_dist(0, population_size - 1);

    return uniform_dist(gen);
}

double getRandomValue(double low, double high) {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> distribution(low, high);
    double randomInteger = distribution(rng);
    return double(randomInteger);
}

class TLBO{
    private:
    int population_size;
    int num_iterations;
    int num_features;
    std::vector<std::vector<double>> population; // Reference to the population used for Metric

    public:
    TLBO(const std::vector<std::vector<double>>& initial_population, int population_size, int num_iterations, int num_features)
        : population_size(population_size), num_iterations(num_iterations), num_features(num_features), population(initial_population) {
    }
    std::pair<double, vector<double>> optimize(Metric& obj) { //vector float -> int
        double best_fitness = 0.0;
        vector<double> fitness;
        for (int i = 0; i < population.size(); i++){
            std::pair<double, vector<double>> pr = obj.metric(population[i]);
            population[i] = pr.second;
            fitness.push_back(pr.first);
        }
        for (int h = 0; h < num_iterations; h++){
            //Teaching phase
            int best_index = 0;
            for (int i = 0; i < fitness.size();i++){
                if (fitness[i] > fitness[best_index])
                    best_index = i;
            }
            vector<double> teacher = population[best_index];
            cout << fitness[best_index];
            vector <double> population_mean = meanAlongAxis(population);

            for (int i = 0; i < population_size;i++){
                if (i != best_index){
                    vector<double> difference = calculateDifference(teacher,population_mean);
                    for (int j = 0; j < difference.size(); j++){
                        difference[j] += population[i][j];
                    }
                    double old_score = fitness[i];
                    std::pair<double,vector<double>> new_sc_d = obj.metric(difference);
                    if (new_sc_d.first > old_score){
                        population[i] = new_sc_d.second;
                        fitness[i] = new_sc_d.first;
                    }
                }
            }

            //learning phase
            for (int i = 0; i < population_size;i++){
                int random_index_1 = 0;
                int random_index_2 = 0;
                while (random_index_1 == random_index_2){
                    random_index_1 = getRandomIndex(population_size);
                    random_index_2 = getRandomIndex(population_size);
                }
                double rand1_sc = fitness[random_index_1],rand2_sc = fitness[random_index_2];
                vector<double> rand1_bl = population[random_index_1], rand2_bl = population[random_index_2];
                vector <double> new_population ;
                if (rand1_sc > rand2_sc){
                    new_population = calculateDifferenceRand(rand1_bl,rand2_bl,population[i]);
                }
                else{
                    new_population = calculateDifferenceRand(rand2_bl,rand1_bl,population[i]);
                }
                double old_score = fitness[i];
                std::pair<double,vector<double>> new_sc_d = obj.metric(new_population);
                if (new_sc_d.first > old_score){
                    population[i] = new_sc_d.second;
                    fitness[i] = new_sc_d.first;
                }  
            }
        }
        
        double max_fitness = 0;
        vector <double> best_agent;
        for (int i = 0; i < fitness.size(); i++){
            if (fitness[i] > max_fitness){
                max_fitness = fitness[i];
                best_agent = population[i];
            }
        }
        std::pair<double,vector<double>> to_ret = std::make_pair(max_fitness,best_agent);
        return to_ret;
    }
};

class SCA{
    private:
    int population_size;
    int num_iterations;
    int num_features;
    std::vector<std::vector<double>> agents; // Reference to the population used for Metric

    public:
    SCA(const std::vector<std::vector<double>>& initial_population, int population_size, int num_iterations, int num_features)
        : population_size(population_size), num_iterations(num_iterations), num_features(num_features), agents(initial_population) {
    }
    std::pair<double, vector<double>> optimize(Metric& obj, int flag = 0, double a_linear_component = 2.0) { //vector float -> int
        double best_fitness = 0.0;
        vector<double> fitness;
        for (int i = 0; i < agents.size(); i++){
            std::pair<double, vector<double>> pr = obj.metric(agents[i]);
            agents[i] = pr.second;
            fitness.push_back(pr.first);
        }

        int best_agent_index  = 0;
        for (int i = 0; i < fitness.size();i++){
                if (fitness[i] > fitness[best_agent_index])
                    best_agent_index = i;
        }
        double best_agent_fitness = fitness[best_agent_index];
        vector <double> best_agent = agents[best_agent_index];

        for (int t = 0; t < num_iterations; t++){
           for (int i = 0; i < agents.size(); i++){
                double a_t = a_linear_component - double(t) * (a_linear_component / double(num_iterations));
                double r1 = getRandomValue(0,1);
                double r2 = getRandomValue(0,1);
                double A = 2 * a_t * r1 - a_t;
                double C = 2 * r2;
                int random_agent_index = getRandomIndex(population_size);
                while (random_agent_index == i){
                   random_agent_index = getRandomIndex(population_size); 
                }
                vector <double> random_agent = agents[random_agent_index];
                vector <double> D = calculateDifferenceSCA(random_agent,agents[i],C);
                vector <double> new_position = calculateDifferenceRandomPositonSCA(random_agent,D,A);

                std::pair<double,vector<double>> now_func_bl = obj.metric(new_position);
                if (now_func_bl.first > fitness[i]){
                    agents[i] = now_func_bl.second;
                    fitness[i] = now_func_bl.first;
                    if (fitness[i] > best_agent_fitness){
                        best_agent_fitness = fitness[i];
                        best_agent = agents[i];
                    }
                }
                if (flag){
                    if (best_agent_fitness > 0){
                        std::pair<double,vector<double>> to_ret = std::make_pair(best_agent_fitness,best_agent);
                        return to_ret;
                    }
                }
            }
           cout << best_agent_fitness;
        }
        std::pair<double,vector<double>> to_ret = std::make_pair(best_agent_fitness,best_agent);
        return to_ret;
    }
};

std::string extracting_dct(std::vector<std::vector<int>> pixel_block, double q = 8.0){
    vector <vector<double>> dct_block = do_dct(pixel_block);
    string s;
    vector<vector<int>> IDX = ret_idx();
    for (const auto& coord : IDX) {
        int i = coord[0], j = coord[1];
        int c0 = sign(dct_block[i][j]) * (q * int(abs(dct_block[i][j]) / q) + (q/2) * (0));
        int c1 = sign(dct_block[i][j]) * (q * int(abs(dct_block[i][j]) / q) + (q/2) * (1));
        if (abs(dct_block[i][j] - c0) < abs(dct_block[i][j] - c1)){
            s += '0';
            if (s == "0")
                return "F";
        }
        else
            s += '1';
    }
    return s;
}

int main() {
    int mode;
    cin >> mode;
    if (mode == 1){
        const string EMBED_ZERO = "0000000000000000000000000000000";
        const int SEARCH_SPACE = 10;
        //opening file 
        ifstream inputFile("to_embed.txt");
        int ind_information = 0;
        string information;
        getline(inputFile,information);
        inputFile.close();
        
        //opening image
        cv::Mat image = cv::imread("lena512.png", cv::IMREAD_GRAYSCALE);
        int rows = image.rows;
        int cols = image.cols;
        std::vector<std::vector<int>> img(rows, std::vector<int>(cols));

        // Copy pixel values from the OpenCV image to the 2D vector
        for (int i = 0; i < rows; i++) 
            for (int j = 0; j < cols; j++) 
                img[i][j] = static_cast<int>(image.at<uchar>(i, j));

        //generate blocks, saving to txt
        vector <int> blocks = generate_blocks(rows*cols/64);
        std::ofstream outputFile("blocks.txt");
        for (int num: blocks)
            outputFile << num << ' ';
        outputFile.close();

        vector<vector<int>> copy_img = img;
        int cnt = 0;
        //for all blocks
        for (int i : blocks){
            //getting image block
            int block_w = i % (rows / 8);
            int block_h = (i - block_w) / (rows / 8);
            vector <vector<int>> pixel_matrix(8,vector<int>(8));
            for (int i1 = block_h * 8 ;i1 < block_h * 8 + 8; i1++)
                for (int i2 = block_w * 8;i2 < block_w * 8 + 8; i2++)
                    pixel_matrix[i1-block_h * 8][i2-block_w*8] = img[i1][i2];
        
            //dct transform
            vector<vector<double>> dct_matrix;
            dct_matrix = do_dct(pixel_matrix);
            
            //embedding info может тут быть проблема с дкт-преобразованием? слишком ровные числа и маленькие
            dct_matrix = embed_to_dct(dct_matrix, string(1, '1') + information.substr(ind_information, 31));
            //undo dct matrix
            vector<vector<int>> new_pixel_matrix = undo_dct(dct_matrix);
            //generating population
            vector<vector<double>> population = generate_population(pixel_matrix,new_pixel_matrix,128,double(0.9),SEARCH_SPACE);
            //metric for block and info
            Metric metric(pixel_matrix,string(1, '1') + information.substr(ind_information, 31), SEARCH_SPACE);
            //tlbo size
            //TLBO tlbo(population,128,128,64);
            SCA sca(population,128,128,64);
            pair <double,vector<double>> solution = sca.optimize(metric);
            cout << solution.first;

            if (solution.first > 1){
                cnt += 1;
                int ind = 0;
                for (int i1 = block_h * 8 ;i1 < block_h * 8 + 8; i1++){
                    for (int i2 = block_w * 8;i2 < block_w * 8 + 8; i2++){
                        copy_img[i1][i2] += solution.second[ind++];
                    }
                }
                //ind_information += 31;
            }
            else{
                cout <<"aboba";
                int searching = 5;
                dct_matrix = embed_to_dct(dct_matrix, string(1, '0') + EMBED_ZERO);
                vector<vector<int>> new_pixel_matrix = undo_dct(dct_matrix);
                vector<vector<double>> population = generate_population(pixel_matrix,new_pixel_matrix,128,double(0.9),searching);
                Metric metric(pixel_matrix,string(1, '0') + EMBED_ZERO, searching);
                SCA sca(population,128,128,64);
                pair <double,vector<double>> solution = sca.optimize(metric,1); 
                int ind = 0;
                for (int i1 = block_h * 8 ;i1 < block_h * 8 + 8; i1++){
                    for (int i2 = block_w * 8;i2 < block_w * 8 + 8; i2++){
                        copy_img[i1][i2] += solution.second[ind++];
                    }
                }
            }
            break;     //delete
        cout <<"aboba";
        }

        //saving image
        // Convert the 2D array to a cv::Mat object
        cv::Mat imageMat(rows, cols, CV_8UC1); // Assuming grayscale image, CV_8UC1 for 1-channel

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                // Set the pixel value in the cv::Mat object
                imageMat.at<uchar>(row, col) = static_cast<uchar>(copy_img[row][col]);
            }
        }
        // Save the image using imwrite
        std::string outputFilePath = "saved.png";
        bool success = cv::imwrite(outputFilePath, imageMat);
    }

    else{
        string bit_string = "";
        //opening image
        cv::Mat image = cv::imread("saved.png", cv::IMREAD_GRAYSCALE);
        int rows = image.rows;
        int cols = image.cols;
        std::vector<std::vector<int>> img(rows, std::vector<int>(cols));

        // Copy pixel values from the OpenCV image to the 2D vector
        for (int i = 0; i < rows; i++) 
            for (int j = 0; j < cols; j++) 
                img[i][j] = static_cast<int>(image.at<uchar>(i, j));
        
        std::ifstream inputFile("blocks.txt");
        std::vector<int> blocks;
        int num;
        // Read data from the file into the vector
        while (inputFile >> num) {
            blocks.push_back(num);
        }
        inputFile.close();
        for (int i : blocks){
            int block_w = i % (rows / 8);
            int block_h = (i - block_w) / (rows / 8);
            vector <vector<int>> pixel_matrix(8,vector<int>(8));
            for (int i1 = block_h * 8 ;i1 < block_h * 8 + 8; i1++)
                for (int i2 = block_w * 8;i2 < block_w * 8 + 8; i2++)
                    pixel_matrix[i1-block_h * 8][i2-block_w*8] = img[i1][i2];

            string s = extracting_dct(pixel_matrix);
            cout << s;
            if (s != "F")
                bit_string += s.substr(1);
            break;
        }

        std::ofstream outputFile("saved.txt");
        outputFile << bit_string;
        outputFile.close(); 
    }
}

