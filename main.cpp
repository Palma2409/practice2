#include "model.h"

int main() {
    unsigned int R1, R2, R3, G1, G2, G3, B1, B2, B3;
    R1 = 7; G1 = 10; B1 = 6; R2 = 9; G2 = 5; B2 = 9; R3 = 6; G3 = 5; B3 = 9;

    unsigned int RGB1 = R1 + G1 + B1;
    unsigned int RGB2 = R2 + G2 + B2;
    unsigned int RGB3G1 = R3 + G3 + B3 + G1;
    unsigned int RGB3B1 = R3 + G3 + B3 + B1;

    //открытие файлов для записи логов
    std::ofstream CFECLog; CFECLog.open("CFEC_log.txt", std::ios::trunc);
    std::ofstream statLog; statLog.open("stat_log.txt", std::ios::trunc);

    //создание объекта модели и установка параметров варианта
    SymModel mySim(R1, RGB1,RGB2, RGB3G1, RGB3B1, &CFECLog, &statLog);
    
    //задание оптимального числа рабочих каждого типа
    mySim.setChannelsParam(4, 4, 1);
    
    //запуск симуляции для конкретного модельного времени
    mySim.simulate(10000);

    CFECLog.close();
    statLog.close();

    return 0;
}
