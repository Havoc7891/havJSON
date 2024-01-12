# havJSON

Havoc's single-file JSON library for C++.

## Table of Contents

- [Features](#features)
- [Getting Started](#getting-started)
- [Usage](#usage)
- [Contributing](#contributing)
- [License](#license)

## Features

- Reading and writing JSON files
  - Also supports generating JSON files from scratch
- Limited BSON file support (Reading, writing, generating)
- Pretty print support when saving a JSON file
- Unicode support

## Getting Started

This library requires C++17. The library should be cross-platform, but has only been tested under Windows so far.

### Installation

Copy the header file into your project folder and include the file like this:

```cpp
#include "havJSON.hpp"
```

### Usage

Here are some code examples demonstrating how to use the library:

#### Read JSON file

```cpp
havJSON::havJSONData root;
havJSON::havJSONStream stream;

if (stream.ParseFile("test.json", root) == false)
{
    return false;
}
```

#### Write JSON file

```cpp
std::shared_ptr<havJSON::havJSONData> root = std::make_shared<havJSON::havJSONData>(havJSON::havJSONDataType::Object);
havJSON::havJSONStream stream;

std::shared_ptr<havJSON::havJSONData> bar = std::make_shared<havJSON::havJSONData>(std::string("Bar"), havJSON::havJSONDataType::String);
root->insert("Foo", std::move(bar));

std::string content;

if (stream.WriteJSONFile("test.json", *root, content, true) == false)
{
    return false;
}
```

#### Read BSON file

```cpp
havJSON::havJSONData root;
havJSON::havJSONStream stream;

if (stream.ParseFile("test.bson", root, havJSON::havJSONType::BSON) == false)
{
    return false;
}
```

#### Write BSON file

```cpp
std::shared_ptr<havJSON::havJSONData> root = std::make_shared<havJSON::havJSONData>(havJSON::havJSONDataType::Object);
havJSON::havJSONStream stream;

std::shared_ptr<havJSON::havJSONData> bar = std::make_shared<havJSON::havJSONData>(std::string("Bar"), havJSON::havJSONDataType::String);
root->insert("Foo", std::move(bar));

std::vector<char> byteContent;

if (stream.WriteBSONFile("test.bson", *root, byteContent) == false)
{
    return false;
}
```

#### Create an array and array entries

```cpp
std::shared_ptr<havJSON::havJSONData> dataObject = std::make_shared<havJSON::havJSONData>(havJSON::havJSONDataType::Object);

std::shared_ptr<havJSON::havJSONData> dataArray = std::make_shared<havJSON::havJSONData>(havJSON::havJSONDataType::Array);

for (int index = 0; index < 10; ++index)
{
    std::shared_ptr<havJSON::havJSONData> arrayDataObject = std::make_shared<havJSON::havJSONData>(havJSON::havJSONDataType::Object);

    arrayDataObject->insert("Test" + std::to_string(index), std::make_shared<havJSON::havJSONData>(index, havJSON::havJSONDataType::Int));

    dataArray->push_back(std::move(arrayDataObject));
}

dataObject->insert("TestArray", std::move(dataArray));

havJSON::havJSONStream stream;

std::string content;

if (stream.WriteJSONFile("test.json", *dataObject, content, true) == false)
{
    return false;
}
```

#### Iterate over an array

```cpp
havJSON::havJSONData root;
havJSON::havJSONStream stream;

if (stream.ParseFile("test.json", root) == false)
{
    return false;
}

havJSON::havJSONData& dataArray = root["TestArray"];

for (int index = 0; index < dataArray.arraySize(); ++index)
{
    havJSON::havJSONData& dataObject = dataArray[index];

    int testValue = dataObject["Test" + std::to_string(index)].toInt();

    std::cout << "Test" << index << ": " << testValue << std::endl;
}
```

#### Check if value exists

```cpp
havJSON::havJSONData root;
havJSON::havJSONStream stream;

if (stream.ParseFile("test.json", root) == false)
{
    return false;
}

if (root.contains("Test") == true)
{
    std::cout << "'Test' exists!" << std::endl;

    std::cout << "Value: " << root["Test"].toString() << std::endl;
}
```

#### Check value type

```cpp
havJSON::havJSONData root;
havJSON::havJSONStream stream;

if (stream.ParseFile("test.json", root) == false)
{
    return false;
}

if (root["Test"].isArray() == false)
{
    std::cout << "'Test' is not an array!" << std::endl;
}
```

## Contributing

Feel free to suggest features or report issues. However, please note that pull requests will not be accepted.

## License

Copyright &copy; 2024 Ren&eacute; Nicolaus

This library is released under the [MIT license](/LICENSE).
