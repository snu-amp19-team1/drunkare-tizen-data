#include <vector>
#include <iostream>
#include <sstream>
#include <string>

//
// This stores data from sensor with `C` channels for `D` seconds.
// TODO: Preprocessing?
//
template <std::size_t C, std::size_t D>
struct Measure {
  static const int _samplingPeriod = 10;       // ms
  static const int _deviceSamplingPeriod = 10; // ms

  constexpr std::size_t _numChannels() { return C; }
  constexpr std::size_t _duration() { return D; }

  int _id;
  int _type;
  int _context;
  size_t _tick, _nextIdx; // tick: deviceSamplingPeriod, nextIdx: samplingPeriod
  bool _done;
  float data[C][D * 1000 / _samplingPeriod];

  Measure(int id, int type, int context)
    : _id(id), _type(type), _context(context), _tick(0), _nextIdx(0), _done(false) {}

  constexpr size_t _size() {
    return 4 + C * D * 1000 / _samplingPeriod;
  }

  size_t _numSamples() const
  {
    return _nextIdx;
  }

  std::string format()
  {
    std::ostringstream oss;
    oss << _id << ',' << _context << ',' << _type << ',';
    for (int c = 0; c < _numChannels(); c++) {
      for (float val : data[c]) {
        oss << val << ',';
      }
    }
    std::string s = oss.str();
    return s.substr(0, s.size()-1);
  }

  std::vector<float> & operator[](std::size_t idx)
  {
    return data[idx];
  }

  const std::vector<float>& operator[](std::size_t idx) const
  {
    return data[idx];
  }

  void tick(const std::vector<float>& readValue)
  {
    if (_done)
      return;

    _tick++;
    if (_tick % (_samplingPeriod / _deviceSamplingPeriod)) {
      return;
    }
    size_t idx = _nextIdx++;
    if (readValue.size() != _numChannels())
      return;

    for (int i = 0; i < _numChannels(); i++) {
      data[i][idx] = readValue[i];
    }

    if (_nextIdx == (_duration() * 1000 / _samplingPeriod)) {
      _done = true;
    }
  }
};
