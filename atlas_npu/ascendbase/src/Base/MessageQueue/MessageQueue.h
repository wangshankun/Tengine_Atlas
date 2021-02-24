/*
 * Copyright (c) 2020.Huawei Technologies Co., Ltd. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ECOSYSTEM_MessageQueue_H
#define ECOSYSTEM_MessageQueue_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <initializer_list>
#include <pthread.h>

using namespace std;

static const int DEFAULT_QUEUE_SIZE = 256;

template<typename T>
class MessageQueue : public queue<T> {
public:
    MessageQueue(mutex &in_lock, condition_variable &in_condition) 
    {
        lock = &in_lock;
        condition = &in_condition;
    };

    ~MessageQueue();

    bool isEmpty() const;

    size_t getSize() const;

    void push(const T &val);

    void push(T &val);

    bool pop();

    T &front();

    const T &front() const;

    T &back();

    const T &back() const;

    void clear();

    condition_variable *condition;
    mutex *lock;

private:
    mutable pthread_mutex_t queue_lock;
};

template<typename T>
MessageQueue<T>::~MessageQueue() 
{
    unique_lock <std::mutex> lk(*lock);
}

template<typename T>
bool MessageQueue<T>::isEmpty() const 
{
    bool result = queue<T>::empty();
    return result;
}

template<typename T>
size_t MessageQueue<T>::getSize() const 
{
    size_t result = queue<T>::size();
    return result;
}

template<typename T>
void MessageQueue<T>::push(T &val) 
{
    queue<T>::push(val);
}


template<typename T>
T &MessageQueue<T>::front() 
{
    T &result = queue<T>::front();
    return result;
}

template<typename T>
bool MessageQueue<T>::pop() 
{
    bool result = false;
    if (!queue<T>::empty()) {
        queue<T>::pop();
        result = true;
    }
    return result;
}

template<typename T>
void MessageQueue<T>::clear() 
{
    mutex *tmpLock = lock;
    condition_variable *cv = condition;
    MessageQueue<T> *empty = new MessageQueue(*tmpLock, *cv);
    queue<T>::swap(*empty);
}

#endif //ECOSYSTEM_MessageQueue_H
