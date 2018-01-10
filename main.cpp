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

const char* help_string =
    "otm-join [OPTION]... FILE1 FILE2\n"
    "\n"
    "Options:\n"
    "    -1 FIELD\n"
    "        Join on this field of file 1\n"
    "    -2 FIELD\n"
    "        Join on this field of file 2\n"
    "    -a FILENUM\n"
    "        Print unpairable rows from file FILENUM\n"
    "    -c FILENUM\n"
    "        Specify that file FILENUM's keys are a correlated subset of the other.\n"
    "        Allows unsorted files with aligned rows/keys to be joined directly.\n"
    "    -j FIELD\n"
    "        Equivalent to -1 FIELD -2 FIELD\n"
    "    -l CHAR\n"
    "        Use CHAR as row separator (default is UNIX newline)\n"
    "    -o FORMAT\n"
    "        Use FORMAT for row output (see below)\n"
    "    -r\n"
    "        Use many-to-one join, rather than one-to-many\n"
    "    -s\n"
    "        Don't trail field separators on unpaired rows\n"
    "    -t CHAR\n"
    "        Use CHAR as field separator (default is tab)\n"
    "    -v FILENUM\n"
    "        Print only unpairable rows from file FILENUM (suppress joins)\n"
    "    -z\n"
    "        Use NUL character as row separator\n"
    "\n"
    "FORMAT is a comma-separated list of fields to output.\n"
    "E.g. 0,1.3,t,2.4\n"
    "  Print the joined field, file1's 3rd field, a blank field, file2's 4th field.\n"
    "The default format is the joined field first, followed by the remaining fields\n"
    "from file 1 and then file 2.\n"
    ;

struct Options {
    const char* filename1;
    const char* filename2;
    bool stdin1;
    bool stdin2;
    bool preserve1;
    bool preserve2;
    bool subset1;
    bool subset2;
    bool showJoin;
    bool manyToOne;
    size_t key1;
    size_t key2;
    char fieldSeparator;
    char lineSeparator;
    bool trail;
    const char* format;

    struct UsageException : public std::runtime_error {
        UsageException(const std::string& what_arg):
            std::runtime_error(what_arg)
        {}
    };

    Options(int argc, char** argv): 
        filename1(""),
        filename2(""),
        stdin1(false),
        stdin2(false),
        preserve1(false),
        preserve2(false),
        subset1(false),
        subset2(false),
        showJoin(true),
        manyToOne(false),
        key1(0),
        key2(0),
        fieldSeparator('\t'),
        lineSeparator('\n'),
        trail(true),
        format("")
    {
        int c;
        while ((c = getopt(argc, argv, "a:v:c:1:2:j:t:l:z:rso:")) != -1) {
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
                    throw UsageException("illegal -a or -v value");
                }
                break;
            case 'c':
                if (strcmp(optarg, "1") == 0) {
                    subset1 = true;
                }
                else if (strcmp(optarg, "2") == 0) {
                    subset2 = true;
                }
                else {
                    throw UsageException("illegal -c value");
                }
                break;
            case '1':
                key1 = size_t(atoi(optarg) - 1); // I have no patience.
                break;
            case '2':
                key2 = size_t(atoi(optarg) - 1); // I have no patience.
                break;
            case 'j':
                key1 = size_t(atoi(optarg) - 1);
                key2 = size_t(atoi(optarg) - 1);
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
            case 'r':
                manyToOne = true;
                break;
            case 's':
                trail = false;
                break;
            case 'o':
                format = optarg;
                break;
            default:
                throw UsageException("bad usage");
            }
        }
        if (argc - optind != 2) {
            throw UsageException("need exactly two files");
        }
        filename1 = argv[optind];
        stdin1 = (strcmp(filename1, "-") == 0);
        filename2 = argv[optind+1];
        stdin2 = (strcmp(filename2, "-") == 0);
        if (stdin1 && stdin2) {
            throw UsageException("cannot use stdin twice");
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

std::vector<FieldSpecification> swap_format_files(const std::vector<FieldSpecification>& original) {
    std::vector<FieldSpecification> swapped(original);
    for (size_t i = 0; i < swapped.size(); i++) {
        switch (original[i].source) {
        case 1:
            swapped[i].source = 2;
            break;
        case 2:
            swapped[i].source = 1;
            break;
        default:
            // Do nothing. We only care about 1 and 2.
            break;
        }
    }
    return swapped;
}

// This function is tightly coupled with join_files().
void join_loop(const Options& options,
               std::istream& file1, // One (not necessarily left file)
               std::istream& file2, // Many (not necessarily right file)
               Line& line1,
               Line& line2,
               const std::vector<FieldSpecification>& format12,
               const std::vector<FieldSpecification>& format1,
               const std::vector<FieldSpecification>& format2,
               size_t key1,
               size_t key2,
               bool subset1,
               bool subset2,
               bool preserve1,
               bool preserve2
               )
{
    if (!line1.eof && !line2.eof) {
        // There are three different alignment modes:
        //   # Correlated subset mode (subset1):
        //       - Left keys are a subset of right keys.
        //       - Matching keys appear in the same order in both files.
        //   # Correlated subset mode (subset2):
        //       - Right keys are a subset of left keys.
        //       - Matching keys appear in the same order in both files.
        //   # Dual-sorted mode (like gjoin):
        //       - Both input files are sorted (using C locale).

        bool sorted_mode = !(subset1 || subset2);
        int order;
        while (true) {
            order = strcmp(&line1.data[line1.columns[key1]], &line2.data[line2.columns[key2]]);
            if (order == 0) {
                // Equal keys found. Exhaust the run of equal keys...
                while (order == 0) {
                    if (options.showJoin) {
                        print_join(options, line1, line2, format12);
                    }
                    line2.advance(options, file2);
                    if (line2.eof) {
                        break;
                    }
                    order = strcmp(&line1.data[line1.columns[key1]], &line2.data[line2.columns[key2]]);
                }
                if (sorted_mode && order > 0) {
                    // This check doesn't catch all file 2 misorderings.
                    throw std::runtime_error("bad ordering on file 2");
                    // file 1 ordering is not checked at all!
                }
                // Don't print - we've just joined
                line1.advance(options, file1);
                if (line1.eof || line2.eof) {
                    break;
                }
                // Neither line is on the same key as before.
            }
            else if (subset2 || (sorted_mode && order < 0)) {
                if (subset1 && subset2) {
                    // If left and right key sets are both subsets of each other, they are the same.
                    // We should never get here if they are the same though.
                    throw std::runtime_error("files do not contain correlating keys");
                }
                if (preserve1) {
                    print_join(options, line1, line2, format1);
                }
                line1.advance(options, file1);
                if (line1.eof) {
                    break;
                }
            }
            else if (subset1 || (sorted_mode && order > 0)) {
                if (preserve2) {
                    print_join(options, line1, line2, format2);
                }
                line2.advance(options, file2);
                if (line2.eof) {
                    break;
                }
            }
            else {
                // Else this code is wrong :P
                throw std::logic_error("reachability assertion failed");
            }
        }
    }

    while (!line1.eof) {
        if (preserve1) {
            print_join(options, line1, line2, format1);
        }
        line1.advance(options, file1);
    }
    while (!line2.eof) {
        if (preserve2) {
            print_join(options, line1, line2, format2);
        }
        line2.advance(options, file2);
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

    if (options.manyToOne) {
        // For the main processing code, file1 must always have unique
        // keys. file2 may have duplicate (but grouped) keys.

        // So, swap everything around.
        join_loop(options,
                  file2,
                  file1,
                  line2,
                  line1,
                  swap_format_files(format12),
                  swap_format_files(format2),
                  swap_format_files(format1),
                  options.key2,
                  options.key1,
                  options.subset2,
                  options.subset1,
                  options.preserve2,
                  options.preserve1
                  );
    } else {
        // Normal, one-to-many
        join_loop(options,
                  file1,
                  file2,
                  line1,
                  line2,
                  format12,
                  format1,
                  format2,
                  options.key1,
                  options.key2,
                  options.subset1,
                  options.subset2,
                  options.preserve1,
                  options.preserve2
                  );
    }
}

int main(int argc, char** argv) {
    try {
        Options options(argc, argv);

        std::ios_base::sync_with_stdio(false);
        std::cin.tie(NULL);

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

        return 0;
    } catch (const Options::UsageException& error) {
        std::cerr << argv[0] << ": " << error.what() << std::endl;
        std::cerr << help_string << std::flush;
        return 1;
    } catch (const std::runtime_error& error) {
        std::cerr << argv[0] << ": " << error.what() << std::endl;
        return 1;
    }
}
