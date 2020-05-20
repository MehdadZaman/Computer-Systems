#include "const.h"
#include "sequitur.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

//MY DECLARATIONS
int process_rule(SYMBOL *rule_head, FILE *out);
int parseSymbolValue(int symbolValue, FILE *out);

int processRestOfByteToSymbol(int bytesLeftToProcess, int symbolValue, FILE *in);
int expandBlock(FILE *out, SYMBOL *ruleHead);

int arrayLength(char **argv);
int validArrayLength(char **argv);
int stringEqual(char *str1, char *str2);
int stringLength(char *str);
int stringToInteger(char *str);

/*
 * You may modify this file and/or move the functions contained here
 * to other source files (except for main.c) as you wish.
 *
 * IMPORTANT: You MAY NOT use any array brackets (i.e. [ and ]) and
 * you MAY NOT declare any arrays or allocate any storage with malloc().
 * The purpose of this restriction is to force you to use pointers.
 * Variables to hold the pathname of the current file or directory
 * as well as other data have been pre-declared for you in const.h.
 * You must use those variables, rather than declaring your own.
 * IF YOU VIOLATE THIS RESTRICTION, YOU WILL GET A ZERO!
 *
 * IMPORTANT: You MAY NOT use floating point arithmetic or declare
 * any "float" or "double" variables.  IF YOU VIOLATE THIS RESTRICTION,
 * YOU WILL GET A ZERO!
 */

/**
 * Main compression function.
 * Reads a sequence of bytes from a specified input stream, segments the
 * input data into blocks of a specified maximum number of bytes,
 * uses the Sequitur algorithm to compress each block of input to a list
 * of rules, and outputs the resulting compressed data transmission to a
 * specified output stream in the format detailed in the header files and
 * assignment handout.  The output stream is flushed once the transmission
 * is complete.
 *
 * The maximum number of bytes of uncompressed data represented by each
 * block of the compressed transmission is limited to the specified value
 * "bsize".  Each compressed block except for the last one represents exactly
 * "bsize" bytes of uncompressed data and the last compressed block represents
 * at most "bsize" bytes.
 *
 * @param in  The stream from which input is to be read.
 * @param out  The stream to which the block is to be written.
 * @param bsize  The maximum number of bytes read per block.
 * @return  The number of bytes written, in case of success,
 * otherwise EOF.
 */
int compress(FILE *in, FILE *out, int bsize) {
    bsize = bsize * 1024;
    if((in == NULL) || (out == NULL))
    {
        return EOF;
    }

    int putCRetValue = 0;
    int fFlushRetValue = 0;

    int numberOfWrittenBytes = 0;

    putCRetValue = fputc(0x81, out);

    if(putCRetValue == EOF)
    {
        return EOF;
    }

    numberOfWrittenBytes++;

    init_rules();
    init_symbols();
    init_digram_hash();

    int bytesPerBlock = 0;

    int readByte = fgetc(in);

    do
    {
        if(readByte == EOF)
        {
            break;
        }

        if(bytesPerBlock == bsize)
        {
            putCRetValue = fputc(0x83, out);

            if(putCRetValue == EOF)
            {
                return EOF;
            }

            numberOfWrittenBytes++;
            SYMBOL *rule_cursor = main_rule;
            do
            {
                int val = process_rule(rule_cursor, out);
                if(val == EOF)
                {
                    return EOF;
                }
                numberOfWrittenBytes += val;
                if((rule_cursor->nextr) != main_rule)
                {
                    putCRetValue = fputc(0x85, out);

                    if(putCRetValue == EOF)
                    {
                        return EOF;
                    }

                    numberOfWrittenBytes++;
                }
                rule_cursor = (rule_cursor->nextr);
            } while(rule_cursor != main_rule);
            putCRetValue = fputc(0x84, out);

            if(putCRetValue == EOF)
            {
                return EOF;
            }

            numberOfWrittenBytes++;
            bytesPerBlock = 0;
            fFlushRetValue = fflush(out);

            if(fFlushRetValue == EOF)
            {
                return EOF;
            }

        }

        if(bytesPerBlock == 0)
        {
            init_rules();
            init_symbols();
            init_digram_hash();
        }

        SYMBOL *tempSymbol = new_symbol(readByte, NULL);

        SYMBOL *previous_symbol;

        if(main_rule == NULL)
        {
            previous_symbol = new_rule(FIRST_NONTERMINAL);
            add_rule(previous_symbol);
            next_nonterminal_value++;
        }

        previous_symbol = (main_rule->prev);
        insert_after(previous_symbol, tempSymbol);
        check_digram(previous_symbol);

        bytesPerBlock++;
    } while((readByte = fgetc(in)) != EOF);

    if(bytesPerBlock != 0)
    {
        if(main_rule != NULL)
        {
            putCRetValue = fputc(0x83, out);

            if(putCRetValue == EOF)
            {
                return EOF;
            }

            numberOfWrittenBytes++;
            SYMBOL *rule_cursor = main_rule;
            do
            {
                int val = process_rule(rule_cursor, out);
                if(val == EOF)
                {
                    return EOF;
                }
                numberOfWrittenBytes += val;
                if((rule_cursor->nextr) != main_rule)
                {
                    putCRetValue = fputc(0x85, out);

                    if(putCRetValue == EOF)
                    {
                        return EOF;
                    }

                    numberOfWrittenBytes++;
                }
                rule_cursor = (rule_cursor->nextr);

            } while(rule_cursor != main_rule);

            putCRetValue = fputc(0x84, out);

            if(putCRetValue == EOF)
            {
                return EOF;
            }

            numberOfWrittenBytes++;
            fFlushRetValue = fflush(out);

            if(fFlushRetValue == EOF)
            {
                return EOF;
            }

        }
        init_rules();
        init_symbols();
        init_digram_hash();
        bytesPerBlock = 0;
    }

    putCRetValue = fputc(0x82, out);

    if(putCRetValue == EOF)
    {
        return EOF;
    }

    numberOfWrittenBytes++;
    fFlushRetValue = fflush(out);

    if(fFlushRetValue == EOF)
    {
        return EOF;
    }

    return numberOfWrittenBytes;
}

int process_rule(SYMBOL *rule_head, FILE *out)
{
    int numberOfWrittenBytes = 0;
    int symbolCount = 0;
    SYMBOL *body_cursor = rule_head;
    do
    {
        int val = parseSymbolValue((body_cursor->value), out);
        if(val == EOF)
        {
            return EOF;
        }
        numberOfWrittenBytes += val;
        body_cursor = (body_cursor->next);
        symbolCount++;
    } while(body_cursor != rule_head);
    if(symbolCount <= 2)
    {
        return EOF;
    }
    return numberOfWrittenBytes;
}

int parseSymbolValue(int symbolValue, FILE *out)
{

    int putCRetValue = 0;

    int byteType = 0;
    int outputByteVale = 0;

    int numberOfWrittenBytes = 0;

    if((symbolValue >= (0x10000)) && (symbolValue <= (0x10FFF)))
    {
        byteType = 4;
        outputByteVale = (symbolValue >> 18);
        outputByteVale = (outputByteVale & 0x07);
        outputByteVale = (outputByteVale | 0xF0);

        putCRetValue = fputc((char)outputByteVale, out);

        if(putCRetValue == EOF)
        {
            return EOF;
        }

        numberOfWrittenBytes++;
    }
    else if((symbolValue >= (0x0800)) && (symbolValue <= (0xFFFF)))
    {
        byteType = 3;
        outputByteVale = (symbolValue >> 12);
        outputByteVale = (outputByteVale & 0x0F);
        outputByteVale = (outputByteVale | 0xE0);

        putCRetValue = fputc((char)outputByteVale, out);

        if(putCRetValue == EOF)
        {
            return EOF;
        }

        numberOfWrittenBytes++;
    }
    else if((symbolValue >= (0x0080)) && (symbolValue <= (0x07FF)))
    {
        byteType = 2;
        outputByteVale = (symbolValue >> 6);
        outputByteVale = (outputByteVale & 0x1F);
        outputByteVale = (outputByteVale | 0xC0);

        putCRetValue = fputc((char)outputByteVale, out);

        if(putCRetValue == EOF)
        {
            return EOF;
        }

        numberOfWrittenBytes++;
    }
    else if((symbolValue >= (0x0000)) && (symbolValue <= (0x007F)))
    {
        byteType = 1;
        outputByteVale = (symbolValue & 0x7F);

        putCRetValue = fputc((char)outputByteVale, out);

        if(putCRetValue == EOF)
        {
            return EOF;
        }

        numberOfWrittenBytes++;
    }
    else
    {
        return EOF;
    }

    int shft_amt = 6 * (byteType - 2);

    for(int i = 0; i < (byteType - 1); i++)
    {
        outputByteVale = (symbolValue >> shft_amt);
        outputByteVale = (outputByteVale & 0x3F);
        outputByteVale = (outputByteVale | 0x80);

        putCRetValue = fputc((char)outputByteVale, out);

        if(putCRetValue == EOF)
        {
            return EOF;
        }

        numberOfWrittenBytes++;

        shft_amt -= 6;
    }

    return numberOfWrittenBytes;
}

/**
 * Main decompression function.
 * Reads a compressed data transmission from an input stream, expands it,
 * and and writes the resulting decompressed data to an output stream.
 * The output stream is flushed once writing is complete.
 *`
 * @param in  The stream from which the compressed block is to be read.
 * @param out  The stream to which the uncompressed data is to be written.
 * @return  The number of bytes written, in case of success, otherwise EOF.
 */
int decompress(FILE *in, FILE *out) {

    if((in == NULL) || (out == NULL))
    {
        return EOF;
    }

    init_rules();
    init_symbols();

    int numberOfWrittenBytes = 0;

    int processingHead = 0;

    int readByte;

    SYMBOL *headSymbol = NULL;
    SYMBOL *currentSymbol = NULL;

    int counter = 0;

    int rule_counter = 0;
    int processing_block_flag = 0;

    while((readByte = fgetc(in)) != EOF)
    {
        int byteType = 0;
        int symbolValue = 0;

        //CHECKS IF IF VALID FILE WITH SOT
        if((counter == 0) && (readByte != 0x81))
        {
            return EOF;
        }
        counter++;

        //SOT
        if(readByte == 0x81)
        {
            if(counter != 1)
            {
                return EOF;
            }
            continue;
        }
        //EOT
        else if(readByte == 0x82)
        {
            if(processing_block_flag == 1)
            {
                return EOF;
            }
            break;
        }
        //RD
        else if(readByte == 0x85)
        {
            if(main_rule == NULL)
            {
                return EOF;
            }
            if(rule_counter == 0)
            {
                return EOF;
            }
            if(processing_block_flag == 0)
            {
                return EOF;
            }
            processingHead = 1;
            currentSymbol = NULL;
            headSymbol = NULL;
            continue;
        }
        //EOB
        else if(readByte == 0x84)
        {
            if(processing_block_flag == 0)
            {
                return EOF;
            }
            int retValue = expandBlock(out, main_rule);
            if(retValue == EOF)
            {
                return EOF;
            }
            numberOfWrittenBytes += retValue;
            init_symbols();
            init_rules();
            processing_block_flag = 0;
            rule_counter = 0;
            continue;
        }
        //SOB
        else if(readByte == 0x83)
        {
            if(processing_block_flag == 1)
            {
                return EOF;
            }
            processing_block_flag = 1;
            processingHead = 1;
            currentSymbol = NULL;
            headSymbol = NULL;
            continue;
        }
        else if((readByte & 0xF8) == 0xF0)
        {
            if(processing_block_flag == 0)
            {
                return EOF;
            }
            byteType = 4;
            symbolValue = readByte & 0x7;
        }
        else if((readByte & 0xF0) == 0xE0)
        {
            if(processing_block_flag == 0)
            {
                return EOF;
            }
            byteType = 3;
            symbolValue = readByte & 0xF;
        }
        else if((readByte & 0xE0) == 0xC0)
        {
            if(processing_block_flag == 0)
            {
                return EOF;
            }
            byteType = 2;
            symbolValue = readByte & 0x1F;
        }
        else if((readByte & 0x80) == 0x0)
        {
            if(processing_block_flag == 0)
            {
                return EOF;
            }
            byteType = 1;
            symbolValue = readByte & 0x7F;
        }
        else
        {
            return EOF;
        }

        symbolValue = processRestOfByteToSymbol((byteType - 1), symbolValue, in);

        if(symbolValue == EOF)
        {
            return EOF;
        }

        if(processingHead == 1)
        {
            headSymbol = new_rule(symbolValue);
            if(headSymbol == NULL)
            {
                return EOF;
            }
            currentSymbol = headSymbol;
            add_rule(currentSymbol);

            (headSymbol->next) = headSymbol;
            (headSymbol->prev) = headSymbol;

            rule_counter++;

            processingHead = 0;
        }
        else if(processingHead == 0)
        {
            if(symbolValue >= FIRST_NONTERMINAL)
            {
                SYMBOL *tempSymbol = new_symbol(symbolValue, headSymbol);
                if(tempSymbol == NULL)
                {
                    return EOF;
                }
                if(currentSymbol != NULL)
                {
                    ((tempSymbol)->prev) = currentSymbol;
                    ((tempSymbol)->next) = headSymbol;
                    ((headSymbol)->prev) = tempSymbol;
                    ((currentSymbol)->next) = tempSymbol;
                    currentSymbol = tempSymbol;
                }
                else
                {
                    currentSymbol = tempSymbol;
                    (currentSymbol->next) = currentSymbol;
                    (currentSymbol->prev) = currentSymbol;
                }
            }
            else if((symbolValue < FIRST_NONTERMINAL) && (symbolValue >= 0))
            {
                SYMBOL *tempSymbol = new_symbol(symbolValue, NULL);
                if(tempSymbol == NULL)
                {
                    return EOF;
                }
                if(currentSymbol != NULL)
                {
                    ((tempSymbol)->prev) = currentSymbol;
                    ((tempSymbol)->next) = headSymbol;
                    ((headSymbol)->prev) = tempSymbol;
                    ((currentSymbol)->next) = tempSymbol;
                    currentSymbol = tempSymbol;
                }
                else
                {
                    currentSymbol = tempSymbol;
                    (currentSymbol->next) = currentSymbol;
                    (currentSymbol->prev) = currentSymbol;
                }
            }
        }
    }

    if(readByte != 0x82)
    {
        return EOF;
    }

    readByte = fgetc(in);

    if(readByte != EOF)
    {
        return EOF;
    }

    int fFlushRetValue = fflush(out);

    if(fFlushRetValue == EOF)
    {
        return EOF;
    }

    return numberOfWrittenBytes;
}

int processRestOfByteToSymbol(int bytesLeftToProcess, int symbolValue, FILE *in)
{
    int validByte = 1;
    int tempSymbolValue = symbolValue;
    for(int i = 0; i < bytesLeftToProcess; i++)
    {
        int currentByte = fgetc(in);
        if((currentByte & 0xC0) != 0x80)
        {
            validByte = -1;
            break;
        }
        int rawValue = currentByte & 0x3F;
        tempSymbolValue = (tempSymbolValue << 6);
        tempSymbolValue = tempSymbolValue | rawValue;
    }
    if(validByte == -1)
    {
        return EOF;
    }
    else
    {
        return tempSymbolValue;
    }
}

int expandBlock(FILE *out, SYMBOL *ruleHead)
{
    int bodyLength = 0;
    int retValue = 0;
    if((ruleHead) == NULL)
    {
        return EOF;
    }

    SYMBOL *symbolCursor = (ruleHead->next);

    while(symbolCursor != ruleHead)
    {
        if((symbolCursor->value) >= FIRST_NONTERMINAL)
        {
            SYMBOL *pointInRuleMap = *(rule_map + (symbolCursor->value));
            int val = expandBlock(out, pointInRuleMap);
            if(val == EOF)
            {
                return EOF;
            }
            retValue += val;
        }
        else
        {
            int putCRetVal = fputc((symbolCursor->value), out);

            if(putCRetVal == EOF)
            {
                return EOF;
            }

            retValue++;
        }

        symbolCursor = (symbolCursor->next);
        bodyLength++;
    }
    if(bodyLength < 2)
    {
        return EOF;
    }
    return retValue;
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the selected program options will be set in the
 * global variable "global_options", where they will be accessible
 * elsewhere in the program.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * Refer to the homework document for the effects of this function on
 * global variables.
 * @modifies global variable "global_options" to contain a bitmap representing
 * the selected options.
 */
int validargs(int argc, char **argv)
{
    global_options = 0;
    int arrLength = arrayLength(argv);
    //CHECK: Invalid number of arguments (too few or too many)
    if((*(argv + argc)) != NULL)
    {
        global_options = 0;
        return -1;
    }

    if(arrLength != argc)
    {
        global_options = 0;
        return -1;
    }

    if(arrLength <= 1)
    {
        global_options = 0;
        return -1;
    }

    //Check if first argument is "-h"
    if(stringEqual(*(argv + 1), "-h") != 0)
    {
        global_options = global_options | 0x1;
        return 0;
    }

    if(((arrLength > 4) || (arrLength <= 0)) || ((argc > 4) || (argc <= 0)))
    {
        global_options = 0;
        return -1;
    }

    //Check if first argument is "-d"
    if((stringEqual(*(argv + 1), "-d") != 0) && (argc == 2))
    {
        global_options = global_options | 0x4;
        return 0;
    }

    //Check if first argument is "-c"
    if(stringEqual(*(argv + 1), "-c") != 0)
    {
         //Check if there are no optional arguments
        if(argc == 2)
        {
            int temp_block_size = 1024;
            temp_block_size = temp_block_size << 16;
            global_options = global_options | temp_block_size;
            global_options = global_options | 0x2;
            return 0;
        }
        if((argc == 4) && (stringEqual((*(argv + 2)), "-b") != 0))
        {
            int blockSize = stringToInteger(*(argv + 3));

            //Check if there are no optional arguments are valid
            if((blockSize >= 1) && (blockSize <= 1024))
            {
                int temp_block_size = blockSize;
                temp_block_size = temp_block_size << 16;
                global_options = global_options | temp_block_size;
                global_options = global_options | 0x2;
                return 0;
            }
        }
    }
    global_options = 0;
    return -1;
}

int arrayLength(char **argv)
{
    int i = 0;
    while((*argv) != NULL)
    {
        if(i >= 10000000)
        {
            break;
        }
        i++;
        argv++;
    }
    return i;
}

int stringEqual(char *str1, char *str2)
{
    int str1Length = stringLength(str1);
    int str2Length = stringLength(str2);

    if(str1Length != str2Length)
    {
        return 0;
    }

    for(int i = 0; i < str1Length; i++)
    {
        if((*str1) != (*str2))
        {
            return 0;
        }
        str1++;
        str2++;
    }

    return 1;
}

int stringLength(char *str)
{
    int i = 0;
    while((*str) != '\0')
    {
        if(i >= 1000)
        {
            break;
        }
        i++;
        str++;
    }
    return i;
}

int stringToInteger(char *str)
{
    int totalInt = 0;
    while((*str) != '\0')
    {
        if(((*str) > '9') ||  ((*str) < '0'))
        {
            return -1;
        }
        totalInt *= 10;
        int temp = (*str) - '0';
        totalInt += temp;
        str++;
    }
    return totalInt;
}