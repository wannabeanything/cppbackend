#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <iostream>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria{
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io},
            strand_(net::make_strand(io_)) {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        
        //auto strand = net::make_strand(io_);
        net::dispatch(strand_, [this, handler]{
            int hot_dog_id = ++hot_dog_counter_;
            auto bread = store_.GetBread();
            auto sausage = store_.GetSausage();
            auto bread_timer = std::make_shared<net::steady_timer>(io_);
            auto sausage_timer = std::make_shared<net::steady_timer>(io_);
            bread->StartBake(*gas_cooker_,[sausage, bread, handler, bread_timer, hot_dog_id](){
                bread_timer->expires_after(HotDog::MIN_BREAD_COOK_DURATION);
                bread_timer->async_wait([bread, sausage, handler, bread_timer, hot_dog_id](sys::error_code ec){
                    if(ec){
                        throw std::runtime_error("Bread didn't bake");
                    }
                    else{
                        bread->StopBaking();
                        if(bread->IsCooked() && sausage->IsCooked()){
                            HotDog hd(hot_dog_id, sausage, bread);
                            Result<HotDog> res(std::move(hd));
                            handler(std::move(res));
                            //std::cout << hot_dog_id << " " << sausage->GetId() << " " << bread->GetId() << std::endl;
                        }
                    }
                });
            });
 
            sausage->StartFry(*gas_cooker_, [bread, sausage, handler, sausage_timer, hot_dog_id]{
                sausage_timer->expires_after(HotDog::MIN_SAUSAGE_COOK_DURATION);
                sausage_timer->async_wait([sausage, bread, handler, sausage_timer, hot_dog_id](sys::error_code ec){
                    if(ec){
                        std::runtime_error("Sausage didn't fry");
                    }
                    else{
                        sausage->StopFry();
                        if(bread->IsCooked() && sausage->IsCooked()){
                            HotDog hd(hot_dog_id, sausage, bread);
                            Result<HotDog> res(std::move(hd));
                            handler(std::move(res));
                            //std::cout << hot_dog_id << " " << sausage->GetId() << " " << bread->GetId() << std::endl;
                        }
                    }
                });
            });
 
 
        });
 
        
    }

private:
    std::atomic_int hot_dog_counter_ = 0;
    net::io_context& io_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};
