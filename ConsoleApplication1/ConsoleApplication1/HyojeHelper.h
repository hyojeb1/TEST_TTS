#pragma once

using std::string;
using std::cout;

static string ToUtf8(const wchar_t* input)
{
	if (input == nullptr)
	{
		return string();
	}

	const int input_size = WideCharToMultiByte(
		CP_UTF8,
		0,
		input,
		-1,
		nullptr,
		0,
		nullptr,
		nullptr
	);

	if (input_size <= 1)
	{
		return string();
	}

	string output(input_size - 1, ' ');
	//string output(input_size, ' ');

	WideCharToMultiByte(
		CP_UTF8,
		0,
		input,
		-1,
		output.data(), // &output[0]
		input_size,
		nullptr,
		nullptr
	);

	return output;
}