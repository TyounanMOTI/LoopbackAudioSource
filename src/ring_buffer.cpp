#include "ring_buffer.h"

ring_buffer::ring_buffer(size_t buffer_size)
  : head(0)
  , tail(0)
  , buffer_full(false)
{
  buffer.assign(buffer_size, 0.0f);
}

const std::pair<segment, segment> ring_buffer::back(size_t length)
{
  segment first, second;

  if (data_length() < length) {
    first.data = nullptr;
    first.length = 0;
    second.data = nullptr;
    second.length = 0;
    return std::make_pair(first, second);
  }

  auto tail_to_end = buffer.size() - tail;
  if (tail_to_end < length) {
    first.length = tail_to_end;
    first.data = &buffer[tail];
    second.length = length - tail_to_end;
    second.data = &buffer[0];
  } else {
    first.length = length;
    first.data = &buffer[tail];
    second.length = 0;
    second.data = nullptr;
  }

  return std::make_pair(first, second);
}

void ring_buffer::pop_back(size_t length)
{
  tail += length;
  if (tail >= buffer.size()) {
    tail -= buffer.size();
  }
  buffer_full = false;
}

size_t ring_buffer::data_length()
{
  if (head == tail) {
    if (buffer_full) {
      return buffer.size();
    } else {
      return 0;
    }
  } else if (head < tail) {
    return buffer.size() - (tail - head);
  } else {
    return head - tail;
  }
}

void ring_buffer::push_front(const segment& input)
{
  auto free_length = buffer.size() - data_length();
  auto head_to_end = buffer.size() - head;
  if (head_to_end > input.length) {
    for (size_t i = 0; i < input.length; ++i) {
      buffer[head + i] = input.data[i];
    }
    head += input.length;
  } else {
    size_t i;
    for (i = 0; i < head_to_end; ++i) {
      buffer[head + i] = input.data[i];
    }
    for (; i < input.length; ++i) {
      buffer[head + i - buffer.size()] = input.data[i];
    }
    head = input.length - head_to_end;
  }
  // ’Ç‚¢‰z‚µ‚½‚çTail‚ðHead‚Éˆê’v‚³‚¹‚é
  if (free_length <= input.length) {
    tail = head;
    buffer_full = true;
  } else {
    buffer_full = false;
  }
}
