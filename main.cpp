#include <iostream>
#include <opencv2/opencv.hpp>
#include <fstream>
#include <string>
#include <random>
#include <vector>
#include <cmath>

using namespace std;

int sign(int x){
    // Функция определяет знак числа
    return (x > 0) - (x < 0);
}

vector <int> generate_blocks(int size){
    // Функция генерирует рандомную перестановку блоков с заданным размером
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
    /*
        Функция преобразует значения пикселей в DCT-coef
        На вход принимается блок изображения
        На выходе блок dct-коэффициентов 
    */

    // Перевод из формата <vector> в формат <Mat>
    cv::Mat inputMat(input.size(), input[0].size(), CV_32S);
    for (size_t i = 0; i < input.size(); i++) {
        for (size_t j = 0; j < input[0].size(); j++) {
            inputMat.at<int>(i, j) = input[i][j];
        }
    }

    // Конвертируем матрицу <Mat> в float
    cv::Mat floatInput;
    inputMat.convertTo(floatInput, CV_32F);

    // Преобразование матрицы в DCT-coef
    cv::Mat dctResult;
    cv::dct(floatInput, dctResult);

    // Сохранение матрицы в виде вектора, вывод вектора
    std::vector<std::vector<double>> output(dctResult.rows, std::vector<double>(dctResult.cols));
    for (int i = 0; i < dctResult.rows; i++) {
        for (int j = 0; j < dctResult.cols; j++) {
            output[i][j] = dctResult.at<float>(i, j);
        }
    }

    return output;
}

std::vector<std::vector<int>> undo_dct(const std::vector<std::vector<double>>& dctCoefficients) {
    /*
        Функция преобразует DCT-coef обратно в пиксели
        На вход получаем блок DCT-coef
        На выходе получаем блок значений пикселей изображения
    */

    // Перевод из формата <vector> в формат <Mat>
    cv::Mat dctResult(dctCoefficients.size(), dctCoefficients[0].size(), CV_64FC1);
    for (size_t i = 0; i < dctCoefficients.size(); i++) {
        for (size_t j = 0; j < dctCoefficients[0].size(); j++) {
            dctResult.at<double>(i, j) = dctCoefficients[i][j];
        }
    }

    // Обратное DCT-преобразование, получаем значения пикселей
    cv::Mat idctResult;
    cv::idct(dctResult, idctResult);

    // Конвертируем из формата <Mat> в <vector> и выводим его
    std::vector<std::vector<int>> output(idctResult.rows, std::vector<int>(idctResult.cols));
    for (int i = 0; i < idctResult.rows; i++) {
        for (int j = 0; j < idctResult.cols; j++) {
            output[i][j] = static_cast<int>(idctResult.at<double>(i, j));
        }
    }

    return output;
}

std::vector<std::vector<double>> embed_to_dct(std::vector<std::vector<double>> dct_matrix, const string bit_string, const char mode = 'A', double q = 20.0){
    /*
    *   Функция реализует встраивание в блок с DCT-coef
    *   На входе:
        dct_matrix - блок dct-coef
        bit_string - встраиваемая строка
        mode - выбранный тип работы, "A" - встроить 32 бита, иначе только 1 бит
        q - шаг квантования
    *   Функция выводит блок dct-coef со встроенными значениями бит
    */
    int ind = 0;
    int cntj = 6;
    for (int i = 0; i < dct_matrix.size(); i++){
        for (int j = dct_matrix[0].size() - 1; j > cntj; j--){
            dct_matrix[i][j] = sign(dct_matrix[i][j]) * (q * int(abs(dct_matrix[i][j]) / q) + (q/2) * (int(bit_string[ind]) - int('0')));
            if (mode != 'A') return dct_matrix; // возвращаем со встроенным одним битом
            ind++;
        }
        if (i == 3) continue; // для обхода только нужных элементов для встраивания
        cntj--;
    }
    return dct_matrix;
}

std::vector<std::vector<double>> generate_population(vector<vector<int>> original,vector<vector<int>> notorig, int population_size = 128,double beta = 0.9,int search_space = 10){
    /*
    *   Функция генерирует популяцию для работы метаэвристик
    *   На входе:
        original - блок изначального изображения
        notorig - блок изображения после встраивание в его DCT-coef (поменялся из-за смены DCT-coef)
        mode - выбранный тип работы, "A" - встроить 32 бита, иначе только 1 бит
        population_size - размер популяции, сколько особей будет в ней
        beta - с какой вероятностью берется значение разности между изначальным блоком и измененным (иначе берется рандом из пространства поиска)
        search_space - пространство поиска, какому интервалу должны соотвествовать значения матрицы изменений 
    *   Функция возвращает популяцию, которая состоит из заданного числа особей
    */

    std::random_device rd;
    std::mt19937 gen(rd());
    double lower_bound = 0.0;
    double upper_bound = 1.0;
    std::uniform_real_distribution<double> uniform_dist(lower_bound, upper_bound);
    std::uniform_int_distribution<int> search_dist(-search_space, search_space);

    // подсчитываем матрицу изменений
    vector<double> diff;
    for (int i = 0; i < original.size(); i++)
        for (int j = 0; j < original[0].size();j++)
            diff.push_back(original[i][j]-notorig[i][j]);
    
    vector<vector<double>> population(population_size,vector<double>(diff.size()));
    
    // генерируем популяцию
    for (int i = 0; i < population_size; i++){
        for (int j = 0; j < diff.size(); j++){
            double random_value = uniform_dist(gen);
            if (random_value > beta){ // если рандом больше вероятности, то заместо значения из матрицы изменений выбираем рандомное
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
    /*
        Функция считает среднее по всем столбцам популяции
        На входе - популяция
        На выходе - вектор средних значения по столбцам
    */
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
    // Функция генерирует случайное целое число
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> distribution(-search_space, search_space);
    int randomInteger = distribution(rng);
    return double(randomInteger);
}

class Metric{
    /*
    *   Класс оценки качества встраивания для данной особи
        Задается параметрами:
        block_matrix - блок из начальной картинки
        bit_string - строка, которую собираемся встраивать
        search_space - пространство поиска, дальше которого значения особи выходить не могут
        mode - встраиваем 1 или несколько бит
    */
    private:
    std::vector<std::vector<int>> block_matrix;
    std::string bit_string;
    int search_space;
    char mode;

    public: 
    Metric(const std::vector<std::vector<int>>& block_matrix, const std::string& bit_string, const int& search_space, const char& mode)
        : block_matrix(block_matrix), bit_string(bit_string), search_space(search_space), mode(mode) {}

    std::pair<double, vector<double>> metric(const std::vector<double>& block, int q = 20) {
        /*
        *   Функция реализует подсчет значения качества для данного блока
        *   На входе:
            block - особь популяции, которая нуждается в проверке качества
            q - заданный шаг квантования еще при встраивании, такой же при извлечении
        *   Функция возвращает пару - преобразованную особь и значение качества для нее
        */

        // New_block - блок после добавлениня к нему матрицы изменений
        // block_flatten - особь, у которой отбросили остаток и проверили на выход за пространство поиска
        std::vector<vector<int>> new_block = block_matrix;
        vector<double> block_flatten = block; 
        for (int i = 0; i < block_flatten.size(); i++){
            block_flatten[i] = floor(block_flatten[i]); // отброс остатка у особи
            if ((block_flatten[i] < -search_space) || (block_flatten[i] > search_space))
                block_flatten[i] = getRandomInteger(search_space); // если значение в особи вышло за пространство - генерируем заместо него новое
        }

        //ind_f1 - индекс, идущий поэлементно в осооби
        int ind_fl = 0;
        for (int i = 0; i < 8; i++){
            for (int j = 0; j < 8; j++){
                new_block[i][j] += block_flatten[ind_fl];
                if (new_block[i][j] > 255){ // выход за предел 255 в изображении, уменьшаем значение особи на разность выхода и 255 
                    int diff = abs(new_block[i][j]-255);
                    block_flatten[ind_fl] -= diff;
                    new_block[i][j] = 255;
                }
                if (new_block[i][j] < 0){ // выход за предел 0, увеличиваем значение особи на значение выхода по модулю
                    int diff = abs(new_block[i][j]);
                    block_flatten[ind_fl] += diff;
                    new_block[i][j] = 0;
                }
                ind_fl++;
            }
        }
        
        // считаем метрику качества psnr
        int sum_elem = 0;
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
                sum_elem += pow(block_matrix[i][j] - new_block[i][j],2);
        double psnr = 10 * log10((pow(8,2) * pow(255,2)) / double(sum_elem));

        //преобразуем получившийся блок картинки в DCT-coef
        vector <vector<double>> dct_block = do_dct(new_block);
        string s;

        // извлекаем из данного блока информацию
        int cntj = 6;
        for (int i = 0; i < dct_block.size(); i++){
            for (int j = dct_block[0].size() - 1; j > cntj; j--){
                int c0 = sign(dct_block[i][j]) * (q * int(abs(dct_block[i][j]) / q) + (q/2) * (0));
                int c1 = sign(dct_block[i][j]) * (q * int(abs(dct_block[i][j]) / q) + (q/2) * (1));
                if (abs(dct_block[i][j] - c0) < abs(dct_block[i][j] - c1))
                    s += '0';
                else
                    s += '1';
                if (s[0] != bit_string[0]){ // несовпадение первого извлеченного бита - нет смысла дальше проверять, возвращаем 0
                    std::pair<double, vector<double>> to_ret = std::make_pair(0.0, block_flatten);
                    return to_ret;
                }
                if (mode == 'Z') break; // должны извлечь только 1 бит
            }
            if (mode == 'Z') break; // должны извлечь только 1 бит
            if (i == 3) continue; // для правильного порядка извлечения 
            cntj--; 
        }

        // подсчитываем кол-во бит, извлеченных правильно
        int cnt = 0;
        for (int i = 0; i < s.length(); i++)
            if (s[i] == bit_string[i])
                cnt += 1;

        //выводим в кач-ве метрики сумму psnr*10^-4 + ber
        std::pair<double, vector<double>> to_ret = std::make_pair(psnr/10000 + double(cnt)/double(s.length()), block_flatten);
        return to_ret;
    }
};


std::vector<double> getRandomArray(size_t size, double low, double high) {
    // Функция генерирует рандомный вектор, состоящий из нецелых чисел
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
    // Функция подсчитывает разность между лучшей особью(учитель) и средним для популяции (метаэвристика TLBO)
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
    // Подсчет разности для рандомных особей (метаэвристика TLBO)
    size_t num_features = population_cur.size();
    std::vector<double> new_population;
    
    std::vector<double> randomUniform1 = getRandomArray(num_features, 0.0, 1.0);
    
    for (size_t i = 0; i < num_features; i++) {
        new_population.push_back(population_cur[i] + randomUniform1[i] * (rand1[i] - rand2[i]));
    }

    return new_population;
}

std::vector<double> calculateDifferenceSCA(const std::vector<double>& random_agent,const std::vector<double>& agent, const double C) {
    // Подсчет разности между агентами в популяции (метаэвристика SCA)
    size_t num_features = agent.size();
    std::vector<double> D;
        
    for (size_t i = 0; i < num_features; i++) {
        D.push_back(abs(C * random_agent[i] - agent[i]));
    }

    return D;
}

std::vector<double> calculateDifferenceRandomPositonSCA(const std::vector<double>& random_agent,const std::vector<double>& D, const double A) {
    // Подсчет разности для рандомных позиций (метаэвристика SCA)
    size_t num_features = D.size();
    std::vector<double> new_position;
        
    for (size_t i = 0; i < num_features; i++) {
        new_position.push_back(random_agent[i] - D[i]*A);
    }

    return new_position;
}

int getRandomIndex(int population_size) {
    // Функция генерирует рандомное целое число - индекс для особи в популяции
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<int> uniform_dist(0, population_size - 1);

    return uniform_dist(gen);
}

double getRandomValue(double low, double high) {
    // Функция генерирует случайное нецелое число в интервале (low,high)
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> distribution(low, high);
    double randomInteger = distribution(rng);
    return double(randomInteger);
}

class TLBO{
    /*
    *   Класс метаэвристики TLBO
        Задается параметрами:
        population_size - размер популяции, по которой нужно итерироваться
        num_iterations - количество поколений, в течение которых выполняется оптимизация метрики
        num_features - количество значений в каждой особи
        population - популяция, с которой нужно будет работать
    */
    private:
    int population_size;
    int num_iterations;
    int num_features;
    std::vector<std::vector<double>> population;

    public:
    TLBO(const std::vector<std::vector<double>>& initial_population, int population_size, int num_iterations, int num_features)
        : population_size(population_size), num_iterations(num_iterations), num_features(num_features), population(initial_population) {
    }

    std::pair<double, vector<double>> optimize(Metric& obj) {
        /*
            Функция реализует оптимизацию метрики с помощью метаэвристики TLBO
            На входе - объект класса метрики
            На выходе - лучшее значение метрики для всех особей в популяции, особь, показывающая лучшее значение метрики
        */

    
        vector<double> fitness; // вектор, содержащий значения кач-ва для каждой особи
        for (int i = 0; i < population.size(); i++){
            std::pair<double, vector<double>> pr = obj.metric(population[i]);
            population[i] = pr.second; // обновление особи после метрики, с учетом ограничений 
            fitness.push_back(pr.first);
        }

        for (int h = 0; h < num_iterations; h++){
            // Стадия учителя
            int best_index = 0;
            for (int i = 0; i < fitness.size();i++)
                if (fitness[i] > fitness[best_index]) // поиск учителя
                    best_index = i;
            
            vector<double> teacher = population[best_index]; // учитель
            vector <double> population_mean = meanAlongAxis(population);

            for (int i = 0; i < population_size;i++){
                if (i != best_index){ // если это не учитель 
                    vector<double> difference = calculateDifference(teacher,population_mean);
                    for (int j = 0; j < difference.size(); j++)
                        difference[j] += population[i][j];
                    
                    double old_score = fitness[i];
                    std::pair<double,vector<double>> new_sc_d = obj.metric(difference);
                    if (new_sc_d.first > old_score){ // проверка, обучил ли учитель ученика 
                        population[i] = new_sc_d.second; // если да - обновляем значение особи и значение метрики для нее 
                        fitness[i] = new_sc_d.first;
                    }
                }
            }

            //стадия ученика 
            for (int i = 0; i < population_size;i++){
                int random_index_1 = 0;
                int random_index_2 = 0;
                while (random_index_1 == random_index_2){ // ищем 2 рандомных учеников
                    random_index_1 = getRandomIndex(population_size);
                    random_index_2 = getRandomIndex(population_size);
                }
                double rand1_sc = fitness[random_index_1],rand2_sc = fitness[random_index_2];
                vector<double> rand1_bl = population[random_index_1], rand2_bl = population[random_index_2];

                vector <double> new_population;
                if (rand1_sc > rand2_sc){ // сравниваем их значения метрики
                    new_population = calculateDifferenceRand(rand1_bl,rand2_bl,population[i]);
                }
                else{
                    new_population = calculateDifferenceRand(rand2_bl,rand1_bl,population[i]);
                }

                double old_score = fitness[i];
                std::pair<double,vector<double>> new_sc_d = obj.metric(new_population);
                if (new_sc_d.first > old_score){ // проверка - лучше ли стало, по сравнению с изначальным
                    population[i] = new_sc_d.second; // если да - обновляем особь, меняем значения метрики
                    fitness[i] = new_sc_d.first;
                }  
            }
        }
        
        // поиск лучшей особи с большим значением метрики
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
    /*
    *   Класс метаэвристики SCA
        Задается параметрами:
        population_size - размер популяции, по которой нужно итерироваться
        num_iterations - количество поколений, в течение которых выполняется оптимизация метрики
        num_features - количество значений в каждой особи
        population - популяция, с которой нужно будет работать
    */
    private:
    int population_size;
    int num_iterations;
    int num_features;
    std::vector<std::vector<double>> agents; // Reference to the population used for Metric

    public:
    SCA(const std::vector<std::vector<double>>& initial_population, int population_size, int num_iterations, int num_features)
        : population_size(population_size), num_iterations(num_iterations), num_features(num_features), agents(initial_population) {
    }

    std::pair<double, vector<double>> optimize(Metric& obj, int flag = 0, double a_linear_component = 2.0) { 
        /*  
            Функция реализует оптимизацию метрики с помощью метаэвристики SCA
            На входе - объект класса метрики, flag - встраиваем 1 бит или несколько (для встраивания бит-флага 0)
            На выходе - лучшее значение метрики для всех особей в популяции, особь, показывающая лучшее значение метрики
        */

        // значения метрики для каждой особи
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
        // поиск лучшего агента и лучшего значения метрики
        double best_agent_fitness = fitness[best_agent_index];
        vector <double> best_agent = agents[best_agent_index];

        // оптимизация метаэвристикой
        for (int t = 0; t < num_iterations; t++){
           for (int i = 0; i < agents.size(); i++){
                double a_t = a_linear_component - double(t) * (a_linear_component / double(num_iterations));
                double r1 = getRandomValue(0,1);
                double r2 = getRandomValue(0,1);
                double A = 2 * a_t * r1 - a_t;
                double C = 2 * r2;
                int random_agent_index = getRandomIndex(population_size);
                while (random_agent_index == i)
                   random_agent_index = getRandomIndex(population_size); 
                
                vector <double> random_agent = agents[random_agent_index];
                vector <double> D = calculateDifferenceSCA(random_agent,agents[i],C);
                vector <double> new_position = calculateDifferenceRandomPositonSCA(random_agent,D,A);

                std::pair<double,vector<double>> now_func_bl = obj.metric(new_position);
                if (now_func_bl.first > fitness[i]){
                    agents[i] = now_func_bl.second;
                    fitness[i] = now_func_bl.first;
                }

                if (flag){ // при встраивании одного бита
                    if (best_agent_fitness > 0){
                        std::pair<double,vector<double>> to_ret = std::make_pair(best_agent_fitness,best_agent);
                        return to_ret;
                    }
                }
            }
        }

        // поиск лучшего агента
        best_agent_fitness = 0;
        for (int i = 0; i < agents.size(); i++){
            std::pair<double, vector<double>> pr = obj.metric(agents[i]);
            agents[i] = pr.second;
            fitness[i] = pr.first;
            if (fitness[i] > best_agent_fitness){
                best_agent_fitness = fitness[i];
                best_agent = agents[i];
            }
        }
        std::pair<double,vector<double>> to_ret = std::make_pair(best_agent_fitness,best_agent);
        return to_ret;
    }
};

double psnr(std::vector<std::vector<int>> original_img,std::vector<std::vector<int>> saved_img){
    /*
        Функция принимает на вход оригинальное изображение и изображение после вставки
        На выходе - значение метрики psnr
    */
    int sum_elem = 0;
    for (int i = 0; i < original_img.size(); i++)
        for (int j = 0; j < original_img[0].size(); j++)
            sum_elem += pow(original_img[i][j] - saved_img[i][j],2);
        
    double psnr = 10 * log10(pow(255,4) / double(sum_elem));
    return psnr;
}

std::string extracting_dct(std::vector<std::vector<int>> pixel_block, double q = 20.0){
    /*
    *   Функция реализует извлечение встроенной информации из блока 
    *   На входе:
        pixel_block - блок изображения, из которого необходимо извлечь информацию
        q - заданный шаг квантования еще при встраивании, такой же при извлечении
    *   Функция возвращает строку - извлеченная информация
    */

    vector <vector<double>> dct_block = do_dct(pixel_block); // перевод блока в DCT-coef
    string s;
    int cntj = 6;

    for (int i = 0; i < dct_block.size(); i++){
        for (int j = dct_block[0].size() - 1; j > cntj; j--){
            int c0 = sign(dct_block[i][j]) * (q * int(abs(dct_block[i][j]) / q) + (q/2) * (0));
            int c1 = sign(dct_block[i][j]) * (q * int(abs(dct_block[i][j]) / q) + (q/2) * (1));
            if (abs(dct_block[i][j] - c0) < abs(dct_block[i][j] - c1)){
                s += '0';
                if (s == "0") // если 1ый выстроенный бит - 0, то в такой блок информацию не встроили
                    return "F"; // возвращаем флаг, что информации в этом блоке нет
            }
            else
                s += '1';
        }
        if (i == 3) continue; // для правильного обхода по элементам блока
        cntj--;
    }

    return s;
}

int main() {
    int mode; // выбор режима - встраивание\извлечение
    cin >> mode;
    if (mode == 1){ // встраивание
        const int SEARCH_SPACE = 10; // пространство поиска

        //открытие файла, что нужно встроить 
        ifstream inputFile("to_embed.txt");
        int ind_information = 0;
        string information;
        getline(inputFile,information);
        inputFile.close();
        
        //открытие картинки
        cv::Mat image = cv::imread("peppers512.png", cv::IMREAD_GRAYSCALE);
        int rows = image.rows;
        int cols = image.cols;
        std::vector<std::vector<int>> img(rows, std::vector<int>(cols));

        // трансформация из формата <Mat> в <vector>
        for (int i = 0; i < rows; i++) 
            for (int j = 0; j < cols; j++) 
                img[i][j] = static_cast<int>(image.at<uchar>(i, j));

        //генерация порядка блоков, сохранение их в файл 
        vector <int> blocks = generate_blocks(rows*cols/64);
        std::ofstream outputFile("blocks.txt");
        for (int num: blocks)
            outputFile << num << ' ';
        outputFile.close();

        vector<vector<int>> copy_img = img;
        int cnt = 0;
        int cnt_blocks = 0;

        for (int i : blocks){
            //получаем блок изображения по известному номера блока
            int block_w = i % (rows / 8);
            int block_h = (i - block_w) / (rows / 8);
            vector <vector<int>> pixel_matrix(8,vector<int>(8));
            for (int i1 = block_h * 8 ;i1 < block_h * 8 + 8; i1++)
                for (int i2 = block_w * 8;i2 < block_w * 8 + 8; i2++)
                    pixel_matrix[i1-block_h * 8][i2-block_w*8] = img[i1][i2];
        
            //трансформация из пикселей в DCT-coef
            vector<vector<double>> dct_matrix;
            dct_matrix = do_dct(pixel_matrix);

            //встраивание информации в DCT-coef блок
            dct_matrix = embed_to_dct(dct_matrix, string(1, '1') + information.substr(ind_information, 31));

            //перевод блока из DCT-coef в пиксельный формат
            vector<vector<int>> new_pixel_matrix = undo_dct(dct_matrix);

            //генерация популяции на основе блока после встраивания и изначального
            vector<vector<double>> population = generate_population(pixel_matrix,new_pixel_matrix,128,double(0.9),SEARCH_SPACE);

            //задаем объект метрики для данного блока и информации для встраивания
            Metric metric(pixel_matrix,string(1, '1') + information.substr(ind_information, 31), SEARCH_SPACE, 'A');
            
            //выбор метаэвристики и оптимизации с помощью нее
            //TLBO meta(population,128,128,64);
            SCA meta(population,128,128,64);
            pair <double,vector<double>> solution = meta.optimize(metric);

            cout << solution.first;
            for (int iz = 0; iz < solution.second.size(); iz++)
                cout << solution.second[iz] << ' '; 

            
            if (solution.first > 1){ // значение кач-ва метрики >1 => информация встроена идеально, сохранем новый блок, добавляя к нему матрицу изменений
                cnt += 1; 
                int ind = 0;
                for (int i1 = block_h * 8 ;i1 < block_h * 8 + 8; i1++){
                    for (int i2 = block_w * 8;i2 < block_w * 8 + 8; i2++){
                        copy_img[i1][i2] += solution.second[ind];
                        ind++;
                    }
                }
                //ind_information += 31; // переход к следующей части информации
            }

            else{ // информация встроена неидеально
                int searching = 5;
                // встраиваем 1 бит - 0
                dct_matrix = embed_to_dct(do_dct(pixel_matrix), string(1, '0'), 'Z');

                // перевод из DCT-coef в пиксели
                vector<vector<int>> new_pixel_matrix = undo_dct(dct_matrix);

                //генерация популяции
                vector<vector<double>> population = generate_population(pixel_matrix,new_pixel_matrix,128,double(0.9),searching);

                //создание объекта метрики, с учетом встраивание 1 бита
                Metric metric(pixel_matrix,string(1, '0'), searching, 'Z');

                // оптимизация с помощью метаэвристики SCA
                SCA sca(population,128,128,64);
                pair <double,vector<double>> solution = sca.optimize(metric,1);

                for (int i1 = 0; i1< 64; i1++)
                        cout << solution.second[i1] <<' ';
                //сохраняем блок, в который не встраивалась информация 
                int ind = 0;
                for (int i1 = block_h * 8 ;i1 < block_h * 8 + 8; i1++){
                    for (int i2 = block_w * 8;i2 < block_w * 8 + 8; i2++){
                        copy_img[i1][i2] += solution.second[ind++];
                    }
                }
            }
        cout <<endl << cnt_blocks++ << ' ';
        }

        //сохраняем изображение
        cv::Mat imageMat(rows, cols, CV_8UC1);
        for (int row = 0; row < rows; ++row) 
            for (int col = 0; col < cols; ++col) 
                imageMat.at<uchar>(row, col) = static_cast<uchar>(copy_img[row][col]);
        std::string outputFilePath = "saved.png";
        bool success = cv::imwrite(outputFilePath, imageMat);

        cout << cnt;
    }

    else{ // извлечение
        string bit_string = "";

        //открываем изображение
        cv::Mat image = cv::imread("saved.png", cv::IMREAD_GRAYSCALE);
        int rows = image.rows;
        int cols = image.cols;
        std::vector<std::vector<int>> img(rows, std::vector<int>(cols));

        // переводим из формата <Mat> в <vector>
        for (int i = 0; i < rows; i++) 
            for (int j = 0; j < cols; j++) 
                img[i][j] = static_cast<int>(image.at<uchar>(i, j));
        
        // открываем файл с порядком блоков
        std::ifstream inputFile("blocks.txt");
        std::vector<int> blocks;
        int num;
        while (inputFile >> num) {
            blocks.push_back(num);
        }
        inputFile.close();

        for (int i : blocks){
            // получаем значения блока изображения по прочитанному номеру блоку
            int block_w = i % (rows / 8);
            int block_h = (i - block_w) / (rows / 8);
            vector <vector<int>> pixel_matrix(8,vector<int>(8));
            for (int i1 = block_h * 8 ;i1 < block_h * 8 + 8; i1++)
                for (int i2 = block_w * 8;i2 < block_w * 8 + 8; i2++)
                    pixel_matrix[i1-block_h * 8][i2-block_w*8] = img[i1][i2];

            // извлекаем информацию из блока
            string s = extracting_dct(pixel_matrix);
            if (s != "F") // информация должна быть извлечена
                bit_string += s.substr(1) + "\n";
        }

        // сохраняем извлеченную информацию в файл
        std::ofstream outputFile("saved.txt");
        outputFile << bit_string;
        outputFile.close(); 

        // октрываем изначальное изображение и считаем метрику psnr между изначальным и получившимся
        cv::Mat image_base = cv::imread("peppers512.png", cv::IMREAD_GRAYSCALE);
        rows = image_base.rows;
        cols = image_base.cols;
        std::vector<std::vector<int>> img_base(rows, std::vector<int>(cols));
        for (int i = 0; i < rows; i++) 
            for (int j = 0; j < cols; j++) 
                img_base[i][j] = static_cast<int>(image_base.at<uchar>(i, j));
        cout << psnr(img_base,img);
    }
}