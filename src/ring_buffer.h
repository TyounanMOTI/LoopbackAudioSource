#pragma once
#include <cstdint>
#include <vector>

struct segment
{
  float* data;
  size_t length;
};

class ring_buffer
{
public:
  ring_buffer(size_t buffer_size);
  void push_front(const segment& input);
  const std::pair<segment, segment> back(size_t length);
  void pop_back(size_t length);

private:
  size_t data_length();
  std::vector<float> buffer;
  size_t head, tail;
  bool buffer_full;
};
