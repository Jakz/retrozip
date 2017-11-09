#pragma once


#include "filters/filters.h"

#include "header.h"
#include <vector>

struct filter_builder
{
  virtual data_source* apply(data_source* source) const = 0;
  virtual data_source* unapply(data_source* source) const = 0;
  virtual box::payload_uid identifier() const = 0;
  virtual std::vector<byte> payload() const = 0;
};

class filter_builder_queue
{
private:
  std::vector<std::unique_ptr<filter_builder>> builders;
  
public:
  /* construct whole payload for all the filters of the queue */
  memory_buffer serializePayload()
  {
    memory_buffer total(256);
    
    for (auto it = builders.begin(); it != builders.end(); ++it)
    {
      const auto& builder = *it;
      
      std::vector<byte> current = builder->payload();
      
      box::Payload payload = { current.size(), builder->identifier(), it != builders.end() - 1 };
      
      total.write(payload);
      total.write(current.data(), 1, current.size());
    }
    
    return total;
  }
  
};
