/**
 * @file types.h
 * @brief Datatypes of IFJ26
 * @author Oleksandr Teslja (xtesljo00)
 */

#ifndef TYPES_H
#define TYPES_H

typedef enum DataType {
    TYPE_UNKNOWN, // for datatypes that will be assigned later by the semantic analyser
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_UNIT,
    TYPE_BOOL,
    TYPE_ANY, // for semantic analysis
} DataType;

#endif