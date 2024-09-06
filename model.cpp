#include "model.h"

void Transact::printEventMessage(std::ofstream& out, const std::string& message) {
    std::time_t t = std::time(0);
    std::tm* now = std::localtime(&t);
    if (out.is_open()) {
        out << now->tm_year + 1900 << '/' << now->tm_mon + 1 << '/' << now->tm_mday << ' ' << now->tm_hour << ':' << now->tm_min << ':' << now->tm_sec << '\t';
        out << "Transact " << this->_ID << " at state " << this->_currState << "; model time: " << this->_model->getTime() << ' ' << message << std::endl;
    }
    else {
        std::cerr << "File opening error" << std::endl;
    }
}

void SymModel::printMessage(std::ofstream& out, const std::string& message) {
    std::string frame = "----------";
    std::time_t t = std::time(0);
    std::tm* now = std::localtime(&t);
    if (out.is_open()) {
        out << frame << std::endl;
        out << now->tm_year + 1900 << '/' << now->tm_mon + 1 << '/' << now->tm_mday << ' ' << now->tm_hour << ':' << now->tm_min << ':' << now->tm_sec << '\t';
        out << message << std::endl;
        out << frame << std::endl;
    }
    else {
        std::cerr << "File opening error" << std::endl;
    }
}

double SymModel::getExponential(double mean) {
    std::random_device rd;
    std::mt19937 gen(rd()); //генератор случайных чисел Мерсенна-Твиста
    std::exponential_distribution<> dist(1. / mean);
    double randomValue = dist(gen);
    return randomValue;
}

void SymModel::removeFromCEC(Transact* transact)
{
    for (auto it = _CEC.begin();; it++)
    {
        if (it == _CEC.end())
            throw std::out_of_range("No such transactansact in CEC");
        if (*it == transact)
        {
            if (it == _CECIt)
                _CECIt++;
            _CEC.erase(it);
            return;
        }
    }
}

void SymModel::insertToFEC(Transact* transact)
{
    for (auto it = _FEC.begin(); it != _FEC.end(); ++it)
    {
        if ((*it)->_endTime > transact->_endTime)
        {
            _FEC.insert(it, transact);
            return;
        }
    }
    _FEC.push_back(transact);
}

void SymModel::processCEC()
{
    while (_CECIt != _CEC.end())
    {
        auto transact = *_CECIt;
        if (!(transact->_blocked))
        {

            transact->_blocked = true;
            if (transact->_currState == 1)
            {
                _queue1.push(transact);
                stats->QueueENTRY[0]++;

                transact->printEventMessage(*(this->CFECLog), "blocked in CEC (Queue1) and added to delay chain");
            }
            else if (transact->_currState == 2)
            {
                _queue2.push(transact);
                stats->QueueENTRY[1]++;
                transact->printEventMessage(*(this->CFECLog), "blocked in CEC (Queue2) and added to delay chain");

            }
            else
                throw std::logic_error("There is transact in CEC with unknown stage");
        }
        _CECIt++;
    }
}

void SymModel::newPhase(int type, Transact* transact, double time)
{
    removeFromCEC(transact);

    //статистики
    stats->OperatorENTRY[type - 1]++;

    transact->_storageType[transact->_currState - 1] = type;
    transact->_startStorageTime[transact->_currState - 1] = _currentTime;

    transact->_endTime = _currentTime + time;

    if (transact->_currState == 1)
    {
        stats->QueueAVETIME[0] += transact->_startStorageTime[0] - transact->_birthTime;
        if (transact->_birthTime == transact->_startStorageTime[0])
            stats->QueueENTRY0[0]++;
        else
            stats->QueueAVETIME0[0] += transact->_startStorageTime[0] - transact->_birthTime;
    }
    if (transact->_currState == 2)
    {
        stats->QueueAVETIME[1] += transact->_startStorageTime[1] - transact->_endStorageTime[0];
        if (transact->_endStorageTime[0] == transact->_startStorageTime[1])
            stats->QueueENTRY0[1]++;
        else
            stats->QueueAVETIME0[1] += transact->_startStorageTime[1] - transact->_endStorageTime[0];
    }

    //добавление транзакта в FEC
    insertToFEC(transact);

    transact->printEventMessage(*(this->CFECLog), "moved to FEC by worker type " + std::to_string(type));

    //освобождаем канал в заданном устройстве
    currFreeChannels[type - 1]--;
}

void SymModel::chooseOperator()
{
    //если свободны устройства 1 типа, помещаем в них транзакт из первой очереди
    while (_queue1.size() && currFreeChannels[0])
    {
        Transact* tr = _queue1.front();
        _queue1.pop();
        newPhase(1, tr, getExponential(this->_RGB1));
    }

    //если свободны устройства 2 типа, помещаем в них транзакт из второй очереди
    while (_queue2.size() && currFreeChannels[1])
    {
        Transact* tr = _queue2.front();
        _queue2.pop();
        newPhase(2, tr, getExponential(this->_RGB2));
    }

    //если заняты устройства 1 и 2 типа, но есть свободные каналы на устройстве 3 типа
    while ((_queue1.size() || _queue2.size()) && currFreeChannels[2])
    {
        if (_queue1.size() >= _queue2.size())
        {
            Transact* tr = _queue1.front();
            _queue1.pop();
            newPhase(3, tr, getExponential(this->_RGB3G1));
        }
        else
        {
            Transact* tr = _queue2.front();
            _queue2.pop();
            newPhase(3, tr, getExponential(this->_RGB3B1));
        }
    }
}

void Transact::nextState()
{
    if (_currState == 0)
    {
        _currState = 1;
        //генерация нового транзакта в FEC
        Transact* transact = new Transact(_model, _model->getTime() + _model->getExponential(_model->_R1));
        _model->insertToFEC(transact);
        transact->printEventMessage(*(_model->CFECLog), "added to FEC");
    }
    else
    {
        _endStorageTime[_currState - 1] = _model->getTime();
        int storage = _storageType[_currState - 1];
        _model->stats->OperatorAVETIME[storage - 1] += _endStorageTime[_currState - 1] - _startStorageTime[_currState - 1];

        //освобождение канала в заданном устройстве
        _model->currFreeChannels[storage - 1] += 1;

        //переходы в следующее состояние
        if (_currState == 2)
        {
            _currState = 3;
            _model->removeFromCEC(this);
            _model->freeTransact(this);
            this->printEventMessage(*(_model->CFECLog), "removed from all chains and terminated");

        }
        if (_currState == 1)
        {
            _currState = 2;
        }
    }
}

//---симуляция---

void SymModel::simulate(double _timeOfSimulation) {

    //очистка данных предыдущей симуляции (если несколько моделирований в одном запуске программы)
    //clear();

    printMessage(*(this->CFECLog), "The simulation has started");

    //инициализирующий модель транзакт
    insertToFEC(new Transact(this, this->getExponential(_R1))); 
    auto initialTransact = _FEC.back();
    initialTransact->printEventMessage(*(this->CFECLog), "(initialize transact) added to FEC");

    //установка итератора в активную позицию цепи текущих событий
    _CECIt = _CEC.end();

    //начало моделирования
    while (true)
    {
        Transact* transact = *(_FEC.begin());

        //обновляем статистику
        getStats(transact, _timeOfSimulation);

        //очистка обработанных транзактов
        terminate();
       
        //завершение симуляции, если получен транзакт, выходящий за рамки времени симуляции
        if (transact->_endTime > _timeOfSimulation)
            break;

        //перемещение транзакта из цепи будущих событий в цепь текущих событий
        _FEC.pop_front();
        _CEC.push_back(transact);

        //итератор цепи текущих событий не должен выходить на её рамки
        if (_CECIt == _CEC.end())
            _CECIt--;

        //проверка корректности модельного времени
        if (_currentTime > transact->_endTime)
            throw std::logic_error("Transact added to FEC with leave time less than current model time");

        //продвижение модельного времени вперёд
        _currentTime = transact->_endTime;
        
        transact->printEventMessage(*(this->CFECLog), "moved to CEC");

        transact->_blocked = false;
        transact->nextState();

        processCEC();
        chooseOperator();
    }

    //передвижение модельного времени до конечного
    _currentTime = _timeOfSimulation; 

    setFinalStats();
    stats->printStats(*(this->statLog));

    //очистка памяти
    clear();

    printMessage(*(this->CFECLog), "Simulation is complete");
}

//---очистка памяти---

void SymModel::terminate() {
    while (_terminated.size())
    {
        delete _terminated.front();
        _terminated.pop();
    }
}

void SymModel::freeTransact(Transact* d) {
    _terminated.push(d);
}

void SymModel::clear()
{
    //очистка и удаление цепей собитий
    for (auto transact : _FEC) { delete transact; }
    for (auto transact : _CEC) { delete transact; }

    _FEC.clear();
    _CEC.clear();

    //очистка очередей
    while (_queue1.size())
        _queue1.pop();
    while (_queue2.size())
        _queue2.pop();

    //обнуление модельного времени
    _currentTime = 0;

    //обнуление статистик
    stats->clearStats();
}

//---статистики---

void SymModel::getStats(Transact* transact, double _timeOfSimulation) {
    double dt = transact->_endTime - _currentTime;
    if (transact->_endTime > _timeOfSimulation) {
        dt = _timeOfSimulation - _currentTime;
    }

    //сбор статистики очередей
    stats->QueueMAX[0] = std::max(stats->QueueMAX[0], (int)_queue1.size());
    stats->QueueMAX[1] = std::max(stats->QueueMAX[1], (int)_queue2.size());
    stats->QueueAVECONT[0] += _queue1.size() * dt;
    stats->QueueAVECONT[1] += _queue2.size() * dt;

    //сбор статистики устройств
    for (int i = 0; i < 3; i++) {
        stats->OperatorUTIL[i] += dt * (_maxChannels[i] - currFreeChannels[i]);
    }
}

void SymModel::setFinalStats()
{
    stats->QueueCONT[0] = _queue1.size();
    stats->QueueCONT[1] = _queue2.size();
    for (int i = 0; i < 2; i++)
    {
        stats->QueueAVECONT[i] /= _currentTime;
        stats->QueueAVETIME[i] /= stats->QueueENTRY[i];
        stats->QueueAVETIME0[i] /= stats->QueueENTRY[i] - stats->QueueENTRY0[i];
    }
    for (int i = 0; i < 3; i++)
    {
        stats->OperatorAVETIME[i] /= stats->OperatorENTRY[i];
        stats->OperatorUTIL[i] /= (_currentTime * _maxChannels[i]);
    }
}

void Stats::printQueueStats(std::ofstream& out) {
    out << std::left << std::setw(10) << "QUEUE" << std::setw(7) << "MAX" << std::setw(9) << "CONT." << std::setw(9) << "ENTRY" 
        << std::setw(9) << "ENTRY(0)" << std::setw(9) << "AVE.CONT." << std::setw(9) << "AVE.TIME" << std::setw(9) << "AVE.(-0)" << std::endl;
    for (int i = 0; i < 2; i++) {
        out << std::left << std::setw(10) << "Q" + std::to_string(i + 1) << std::setw(7) << this->QueueMAX[i] << std::setw(9) << this->QueueCONT[i] 
            << std::setw(9) << this->QueueENTRY[i] << std::setw(9) << this->QueueENTRY0[i] << std::setw(9) << this->QueueAVECONT[i] 
            << std::setw(9) << this->QueueAVETIME[i] << std::setw(9) << this->QueueAVETIME0[i] << std::endl;
    }
}

void Stats::printStorageStats(std::ofstream& out) {
    out << std::left << std::setw(15) << "OPERATOR" << std::setw(7) << "CAP." << std::setw(9) << "ENTRY" 
        << std::setw(9) << "AVE.TIME" << std::setw(9) << "UTIL." << std::endl;
    for (int i = 0; i < 3; i++) {
        out << std::left << std::setw(15) << "W" + std::to_string(i + 1) << std::setw(7) << _model->_maxChannels[i] 
            << std::setw(9) << this->OperatorENTRY[i] << std::setw(9) << this->OperatorAVETIME[i] 
            << std::setw(9) << this->OperatorUTIL[i] << std::endl;
    }
}

void Stats::printStats(std::ofstream& out){
    std::string frame = "----------";
    std::time_t t = std::time(0);
    std::tm* now = std::localtime(&t);
    if (out.is_open()) {
        out << frame << std::endl;
        out << now->tm_year + 1900 << '/' << now->tm_mon + 1 << '/' << now->tm_mday 
            << ' ' << now->tm_hour << ':' << now->tm_min << ':' << now->tm_sec << std::endl;
        out << frame << std::endl;
        printQueueStats(out);
        out << frame << std::endl;
        printStorageStats(out);
    } else {
        std::cerr << "File opening error" << std::endl;
    }
}

