#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cmath>
#include <functional>

/**
 * Máquina virtual y evaluador de expresiones matemáticas EEL2 (Jesusonic FX de REAPER).
 * Permite compilar y ejecutar código DSP en tiempo real sobre muestras de audio.
 */
class NovaJsfxVm {
public:
    NovaJsfxVm();
    ~NovaJsfxVm();

    enum OpCode {
        OP_NOP = 0,
        OP_PUSH_CONST,
        OP_LOAD_VAR,
        OP_STORE_VAR,
        OP_LOAD_MEM,
        OP_STORE_MEM,
        OP_ADD,
        OP_SUB,
        OP_MUL,
        OP_DIV,
        OP_MOD,
        OP_POW,
        OP_NEG,
        OP_EQ,
        OP_NEQ,
        OP_LT,
        OP_LTE,
        OP_GT,
        OP_GTE,
        OP_AND,
        OP_OR,
        OP_NOT,
        OP_CALL_FUNC,
        OP_JUMP_IF_ZERO,
        OP_JUMP,
        OP_POP
    };

    struct Instruction {
        OpCode op = OP_NOP;
        double value = 0.0;
        int varIndex = -1;
        int targetIp = -1;
        int funcId = 0;
    };

    using Program = std::vector<Instruction>;

    // Compila código fuente EEL2 en un programa de bytecode
    bool compile(const std::string& source, Program& outProgram, std::string* outError = nullptr);

    // Ejecuta un programa compilado
    void execute(const Program& program);

    // Acceso y registro de variables por nombre
    int getOrRegisterVariable(const std::string& name);
    double getVariable(int index) const;
    void setVariable(int index, double value);
    double getVariableByName(const std::string& name) const;
    void setVariableByName(const std::string& name, double value);

    // Memoria RAM virtual compartida de JSFX (hasta 8 millones de slots)
    double readMem(size_t index) const;
    void writeMem(size_t index, double value);
    void clearMemory();

private:
    std::unordered_map<std::string, int> m_varNameToIndex;
    std::vector<std::string> m_varIndexToName;
    std::vector<double> m_variables;

    // Memoria RAM de JSFX
    std::vector<double> m_ram;

    // Pila de evaluación
    mutable std::vector<double> m_stack;

    // Funciones matemáticas
    double callMathFunc(int funcId, double arg1, double arg2 = 0.0);
};
