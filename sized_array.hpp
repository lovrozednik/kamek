#pragma once

class sized_array
{
public:
    unsigned char *data = nullptr;
    unsigned int length;

    sized_array(unsigned char *_data, unsigned int _length)
    {
        data = _data;
        length = _length;
    }

    sized_array()
    {
        data = nullptr;
        length = 0;
    }

    sized_array(unsigned int _length)
    {
        length = _length;
        data = new unsigned char[_length];
    }
    ~sized_array()
    {
        delete data;
    }
};