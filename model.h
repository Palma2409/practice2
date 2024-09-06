#include <iostream>
#include <vector>
#include <list>
#include <queue>
#include <random>
#include <chrono>
#include <sstream>
#include <fstream>
#include <string>
#include <iomanip>

class SymModel;	//упреждающее объявление главного класса

class Transact {
	
	friend class SymModel;			//класс используемый для моделирования
	SymModel* _model; 

	unsigned int _ID;				//уникальный номер транзакта
	bool _blocked;					//блокировка транзакта используется для ускорения симуляции (занятые транзакты игнорируются)
	int _storageType[2];			//типы устройств на каждой фазе

	unsigned int _currState;		//0 - рождение транзакта (birth), 
									//1 - первая фаза обработки
									//2 - вторая фаза обработки
									//3 - выход из модели (terminate)

	double _birthTime, _endTime;	//время рождения и выхода из модели
	double _startStorageTime[2],	//время входа в устройство на каждой фазе
		_endStorageTime[2];			//время выхода из устройства на каждой фазе

public:
	Transact(SymModel* model, double birthTime) : _model(model), _birthTime(birthTime), _endTime(birthTime),
		_currState(0), _blocked(false) { static unsigned int _ID = 0; this->_ID = ++_ID; }

	//переход к следующему состоянию
	void nextState();

	//печать сообщения для логов FEC/CEC
	void printEventMessage(std::ofstream& out, const std::string& message);
};

class Stats {
	SymModel* _model;
public:
	Stats(SymModel* model) : _model(model) {};

	//Статистики очередей
	int QueueMAX[2],			//максимальная длина очереди
		QueueCONT[2],			//текущая длина очереди
		QueueENTRY[2],			//общее число заходов в очередь
		QueueENTRY0[2];			//число нулевых заходов в очередь
	double QueueAVECONT[2],		//средняя длина очереди
		QueueAVETIME[2],		//среднее время пребывания в очереди
		QueueAVETIME0[2];		//среднее время пребывания в очереди без учёта нулевых входов

	//Статистики устройств
	int OperatorENTRY[3];		//общее число заходов в устройство
	double OperatorUTIL[3],		//коэффициент использования устройства
		OperatorAVETIME[3];		//среднее время пребывание в устройстве

	//очистка статистики
	void clearStats() {
		for (int i = 0; i < 3; i++)
		{
			OperatorAVETIME[i] = 0;
			OperatorENTRY[i] = 0;
			OperatorUTIL[i] = 0;
			if (i < 2)
			{
				QueueAVECONT[i] = 0;
				QueueAVETIME0[i] = 0;
				QueueMAX[i] = 0;
				QueueAVETIME0[i] = 0;
				QueueCONT[i] = 0;
				QueueENTRY[i] = 0;
				QueueENTRY0[i] = 0;
			}
		}
	}

	//вывод статистики на экран
	void printQueueStats(std::ofstream& out);
	void printStorageStats(std::ofstream& out);
	void printStats(std::ofstream& out);
};

class SymModel {
	friend Stats;

	double _currentTime;											//модельное время
	unsigned int _maxChannels[3];									//число каналов в устройствах каждого типа

	std::queue<Transact*> _terminated;								//прошедшие две фазы транзакты к удалению
	std::list<Transact*> _FEC, _CEC;								//цепи текущих и будущих событий
	std::list<Transact*>::iterator _CECIt;							//итератор для цепи текущих событий
	std::queue<Transact*> _queue1, _queue2;							//очереди к устройствам на 1 и 2 фазах

	void newPhase(int type, Transact* tr, double time);				//назначение транзакта устройству и перемещение транзакт в FEC
	void chooseOperator();											//выбор типа устройства в зависимости от длин очередей
	void processCEC();												//распределение транзактов из CEC по очередям
	void setFinalStats();											//расчёт итоговой статистики
	void clear();													//очистка памяти
public:
	SymModel(unsigned int R1, unsigned int RGB1, unsigned int RGB2, unsigned int RGB3G1, unsigned int RGB3B1, std::ofstream* _CFECLog = nullptr,
		std::ofstream* _statLog = nullptr) : _R1(R1), _RGB1(RGB1), _RGB2(RGB2), _RGB3G1(RGB3G1), _RGB3B1(RGB3B1),
		CFECLog(_CFECLog), statLog(_statLog) {
		stats = new Stats(this);
	}
	
	//число свободных каналов на каждом из устройств
	unsigned int currFreeChannels[3];
	
	//параметры варианта для эксп. распределений
	unsigned int _R1, _RGB1, _RGB2, _RGB3G1, _RGB3B1;

	//статистика
	Stats* stats;

	//логи цепей событий и статистики
	std::ofstream* CFECLog;
	std::ofstream* statLog;

	//поместить транзакт в блок TERMINATE
	void freeTransact(Transact* d); 

	//удалить все терминированные транзакты
	void terminate();

	//варьировать число каналов в устройстве каждого типа
	void setChannelsParam(unsigned int W1, unsigned int W2, unsigned int W3) {
		if (W1 >= 1 && W2 >= 1 && W3 >= 1) {
			_maxChannels[0] = W1, _maxChannels[1] = W2, _maxChannels[2] = W3;
			currFreeChannels[0] = W1, currFreeChannels[1] = W2, currFreeChannels[2] = W3;
		}
		else
			throw std::logic_error("At least one channel is required for each storage");
	}

	double getTime() { return _currentTime; }
	double getExponential(double);
	void getStats(Transact*, double);
	
	//перемещение транзактов между цепями событий
	void insertToFEC(Transact*);
	void removeFromCEC(Transact*);

	//основная функция симуляции
	void simulate(double);

	void printMessage(std::ofstream& out, const std::string& message);
};
