// A one-to-many join program.
// Does not support cross-joining.

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <stdexcept>

struct Options {
    const char* filename1;
    const char* filename2;
    bool stdin1;
    bool stdin2;
    bool preserve1;
    bool preserve2;
    bool showJoin;
    size_t key1;
    size_t key2;
    char fieldSeparator;
    char lineSeparator;
    bool trail;
    const char* format;

    Options(int argc, char** argv): 
        filename1(""),
        filename2(""),
        stdin1(false),
        stdin2(false),
        preserve1(false),
        preserve2(false),
        showJoin(true),
        key1(0),
        key2(0),
        fieldSeparator('\t'),
        lineSeparator('\n'),
        trail(true),
        format("")
    {
        int c;
        while ((c = getopt(argc, argv, "a:v:1:2:t:l:z:so:")) != -1) {
            switch (c) {
            case 'v':
            case 'a':
                if (c == 'v') {
                    showJoin = false;
                }
                if (strcmp(optarg, "1") == 0) {
                    preserve1 = true;
                }
                else if (strcmp(optarg, "2") == 0) {
                    preserve2 = true;
                }
                else {
                    throw std::runtime_error("illegal -a or -v value");
                }
                break;
            case '1':
                key1 = size_t(atoi(optarg) - 1); // I have no patience.
                break;
            case '2':
                key2 = size_t(atoi(optarg) - 1); // I have no patience.
                break;
            case 't':
                fieldSeparator = optarg[0];
                break;
            case 'l':
                lineSeparator = optarg[0];
                break;
            case 'z':
                lineSeparator = '\0';
                break;
            case 's':
                trail = false;
                break;
            case 'o':
                format = optarg;
                break;
            default:
                throw std::runtime_error("unknown option");
            }
        }
        if (argc - optind != 2) {
            throw std::runtime_error("need exactly two files");
        }
        filename1 = argv[optind];
        stdin1 = (strcmp(filename1, "-") == 0);
        filename2 = argv[optind+1];
        stdin2 = (strcmp(filename2, "-") == 0);
        if (stdin1 && stdin2) {
            throw std::runtime_error("cannot use stdin twice");
        }
    }
};

struct Line {
    bool eof;
    std::vector<char> data;
    std::vector<size_t> columns;
    std::vector<size_t> lengths;

    Line():
        eof(false),
        data(1024),
        columns(1024),
        lengths(1024)
    {
    }

    void advance(const Options& options, std::istream& file) {
        if (eof) {
            return;
        }
        data.clear();
        columns.clear();
        lengths.clear();

        int c;
        int fieldStart = 0;
        while (true) {
            c = file.get();

            if (file.eof()) {
                if (data.size() > 0) {
                    std::cerr << "improperly terminated line" << std::endl;
                    data.push_back(0);
                    columns.push_back(fieldStart);
                    lengths.push_back(data.size() - fieldStart - 1);
                    fieldStart = data.size();
                } else {
                    eof = true;
                }
                break;
            }
            if (file.fail()) {
                throw std::runtime_error("IO error");
            }
            if (c == options.lineSeparator) {
                data.push_back(0);
                columns.push_back(fieldStart);
                lengths.push_back(data.size() - fieldStart - 1);
                fieldStart = data.size();
                break;
            }
            else if (c == options.fieldSeparator) {
                data.push_back(0);
                columns.push_back(fieldStart);
                lengths.push_back(data.size() - fieldStart - 1);
                fieldStart = data.size();
            } else {
                data.push_back(c);
            }
        }
    }
    // int advance(const Options& options, std::istream& file) {
    //     data.clear();
    //     columns.clear();
    //     size_t got = file.getline(data.data(), data.capacity(), options.lineSeparator).gcount();
    //     if (file.eof()) {
    //         eof = true;
    //     }
    //     if (file.fail()) {
    //         throw std::runtime_error("IO error");
    //     }
    //     if (file.gcount() == data.capacity()) {
    //         // Slower, but rarely runs:
    //         int c;
    //         while ((c = file.get()) != options.lineSeparator) {
    //             if (file.fail()) {
    //                 throw std::runtime_error("IO error");
    //             }
    //             if (file.eof()) {
    //                 break;
    //             }
    //             got++;
    //             if (
    //             data.push(c);
    //         }
    //     }
    //     for (int i = 0; i
    // }
};

struct FieldSpecification {
    int source;
    size_t column;

    FieldSpecification(int source, size_t column):
        source(source),
        column(column)
    {}
};

void print_join(const Options& options, const Line& line1, const Line& line2, const std::vector<FieldSpecification>& format) {
    bool first = true;
    for (size_t i = 0; i < format.size(); i++) {
        if (!first) {
            std::cout.put(options.fieldSeparator);
        }
        first = false;
        switch (format[i].source) {
        case 0:
            // Nothing
            break;
        case 1:
            if (format[i].column < line1.columns.size()) {
                std::cout.write(&line1.data[line1.columns[format[i].column]], line1.lengths[format[i].column]);
            }
            break;
        case 2:
            if (format[i].column < line2.columns.size()) {
                std::cout.write(&line2.data[line2.columns[format[i].column]], line2.lengths[format[i].column]);
            }
            break;
        }
    }
    std::cout.put(options.lineSeparator);
}

void get_formats(const Options& options,
                 const Line& line1,
                 const Line& line2,
                 std::vector<FieldSpecification>& format12 /* out */, 
                 std::vector<FieldSpecification>& format1  /* out */, 
                 std::vector<FieldSpecification>& format2  /* out */)
{
    if (strcmp(options.format, "") != 0) {
        size_t i = 0;
        size_t limit = strlen(options.format);
        while(i < limit) {
            switch (options.format[i]) {
            case '0':
                format12.push_back(FieldSpecification(1, options.key1));
                format1.push_back(FieldSpecification(1, options.key1));
                format2.push_back(FieldSpecification(2, options.key2));
                i++;
                if (options.format[i] != ',' && options.format[i] != '\0') {
                    throw std::runtime_error("illegal output format");
                }
                i++; // ,
                break;
            case 't':
                format12.push_back(FieldSpecification(0, 0));
                format1.push_back(FieldSpecification(0, 0));
                format2.push_back(FieldSpecification(0, 0));
                i++;
                if (options.format[i] != ',' && options.format[i] != '\0') {
                    throw std::runtime_error("illegal output format");
                }
                i++; // ,
                break;
            case '1':
            case '2':
                {
                    int l = options.format[i] - '0';
                    i++;
                    if (options.format[i] != '.') {
                        throw std::runtime_error("illegal output format");
                    }
                    i++;
                    if (options.format[i] == ',' || options.format[i] == '\0') {
                        throw std::runtime_error("illegal output format");
                    }
                    int c = 0;
                    for (; options.format[i] != ',' && options.format[i] != '\0'; i++) {
                        if (options.format[i] < '0' || '9' < options.format[i]) {
                            throw std::runtime_error("illegal output format");
                        }
                        c *= 10;
                        c += options.format[i] - '0';
                    }
                    c--; // 1 indexing to 0 indexing
                    format12.push_back(FieldSpecification(l, c));
                    if (l == 1) {
                        format1.push_back(FieldSpecification(l, c));
                        format2.push_back(FieldSpecification(0, 0));
                    } else {
                        format1.push_back(FieldSpecification(0, 0));
                        format2.push_back(FieldSpecification(l, c));
                    }
                    if (options.format[i] != ',' && options.format[i] != '\0') {
                        throw std::runtime_error("illegal output format");
                    }
                    i++;
                }
                break;
            default:
                throw std::runtime_error("illegal output format");
            }
        }
    } else {
        // Push instead of emplace to support older compilers.
        format12.push_back(FieldSpecification(1, options.key1));
        format1.push_back(FieldSpecification(1, options.key1));
        format2.push_back(FieldSpecification(2, options.key2));
        for (size_t i = 0; i < line1.columns.size(); i++) {
            if (i != options.key1) {
                format12.push_back(FieldSpecification(1, i));
                format1.push_back(FieldSpecification(1, i));
                format2.push_back(FieldSpecification(0, 0));
            }
        }
        for (size_t i = 0; i < line2.columns.size(); i++) {
            if (i != options.key2) {
                format12.push_back(FieldSpecification(2, i));
                if (options.trail) {
                    format1.push_back(FieldSpecification(0, 0));
                }
                format2.push_back(FieldSpecification(2, i));
            }
        }
    }
}

void join_files(const Options& options, std::istream& file1, std::istream& file2) {
    Line line1;
    Line line2;

    std::vector<FieldSpecification> format12;
    std::vector<FieldSpecification> format1;
    std::vector<FieldSpecification> format2;

    line1.advance(options, file1);
    line2.advance(options, file2);

    get_formats(options, line1, line2, format12, format1, format2);

    int order;
    while (true) {
        order = strcmp(&line1.data[line1.columns[options.key1]], &line2.data[line2.columns[options.key2]]);
        if (order < 0) {
            if (options.preserve1) {
                print_join(options, line1, line2, format1);
            }
            line1.advance(options, file1);
            if (line1.eof) {
                break;
            }
        }
        else if (order > 0) {
            if (options.preserve2) {
                print_join(options, line1, line2, format2);
            }
            line2.advance(options, file2);
            if (line2.eof) {
                break;
            }
        }
        else {
            while (order == 0) {
                if (options.showJoin) {
                    print_join(options, line1, line2, format12);
                }
                line2.advance(options, file2);
                if (line2.eof) {
                    break;
                }
                order = strcmp(&line1.data[line1.columns[options.key1]], &line2.data[line2.columns[options.key2]]);
            }
            if (line2.eof) {
                break;
            }
            if (order < 0) {
                // Don't print - we've just joined
                line1.advance(options, file1);
                if (line2.eof) {
                    break;
                }
            } else {
                throw std::runtime_error("bad ordering on file 2");
                // file 1 ordering is not checked!
            }
        }
    }

    if (!line1.eof && order == 0) {
        // In case we just did a join.
        // We will have joined on some line2s, so we will have already used the current line1.
        line1.advance(options, file1);
    }
    while (!line1.eof) {
        if (options.preserve1) {
            print_join(options, line1, line2, format1);
        }
        line1.advance(options, file1);
    }
    while (!line2.eof) {
        if (options.preserve2) {
            print_join(options, line1, line2, format2);
        }
        line2.advance(options, file2);
    }
}

int main(int argc, char** argv) {
    Options options(argc, argv);

    std::ios_base::sync_with_stdio(false);

    std::istream* file1 = options.stdin1 ? &std::cin : new std::ifstream(options.filename1);
    if (file1->fail()) {
        delete file1;
        throw std::runtime_error("failed to open file1");
    }

    std::istream* file2 = options.stdin2 ? &std::cin : new std::ifstream(options.filename2);
    if (file2->fail()) {
        delete file2;
        throw std::runtime_error("failed to open file2");
    }

    join_files(options, *file1, *file2);

    if (file1 != &std::cin) {
        delete file1;
    }
    if (file2 != &std::cin) {
        delete file2;
    }

    // std::istream* file1;
    // std::istream* file2;
    // if (options.stdin1) {
    //     file1 = &std::cin;
    // } else {
    //     std::ifstream* file = new std::ifstream(options.filename1);
    //     if (file.fail()) {
    //         delete file;
    //         throw std::runtime_error("failed openning file 1");
    //     }
    //     file1 = file;
    // }
    // if (options.stdin2) {
    //     file2 = &std::cin;
    // } else {
    //     std::ifstream* file = new std::ifstream(options.filename2);
    //     if (file.fail()) {
    //         delete file;
    //         throw std::runtime_error("failed openning file 2");
    //     }
    //     file2 = file;
    // }

    // join_files(options, *file1, *file2);

    // if (!options.stdin1) {
    //     delete file1;
    // }
    // if (!options.stdin2) {
    //     delete file2;
    // }

    return 0;
}
