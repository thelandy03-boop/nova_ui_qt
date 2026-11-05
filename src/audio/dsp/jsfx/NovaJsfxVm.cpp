#include "audio/dsp/jsfx/NovaJsfxVm.h"
#include <cctype>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum MathFuncId {
    FN_SIN = 1,
    FN_COS,
    FN_TAN,
    FN_ASIN,
    FN_ACOS,
    FN_ATAN,
    FN_ATAN2,
    FN_SQRT,
    FN_POW,
    FN_EXP,
    FN_LOG,
    FN_LOG10,
    FN_ABS,
    FN_MIN,
    FN_MAX,
    FN_FLOOR,
    FN_CEIL,
    FN_SIGN,
    FN_TANH,
    FN_RAND
};

static int getMathFuncId(const std::string& name) {
    if (name == "sin") return FN_SIN;
    if (name == "cos") return FN_COS;
    if (name == "tan") return FN_TAN;
    if (name == "asin") return FN_ASIN;
    if (name == "acos") return FN_ACOS;
    if (name == "atan") return FN_ATAN;
    if (name == "atan2") return FN_ATAN2;
    if (name == "sqrt") return FN_SQRT;
    if (name == "pow") return FN_POW;
    if (name == "exp") return FN_EXP;
    if (name == "log") return FN_LOG;
    if (name == "log10") return FN_LOG10;
    if (name == "abs") return FN_ABS;
    if (name == "min") return FN_MIN;
    if (name == "max") return FN_MAX;
    if (name == "floor") return FN_FLOOR;
    if (name == "ceil") return FN_CEIL;
    if (name == "sign") return FN_SIGN;
    if (name == "tanh") return FN_TANH;
    if (name == "rand") return FN_RAND;
    return 0;
}

NovaJsfxVm::NovaJsfxVm()
{
    // Reservar memoria inicial
    m_ram.resize(65536, 0.0);
    m_stack.reserve(256);

    // Registrar variables integradas de REAPER JSFX
    getOrRegisterVariable("spl0");
    getOrRegisterVariable("spl1");
    getOrRegisterVariable("srate");
    getOrRegisterVariable("tempo");
    getOrRegisterVariable("samplesblock");
    getOrRegisterVariable("play_position");

    setVariableByName("srate", 48000.0);
    setVariableByName("tempo", 120.0);
}

NovaJsfxVm::~NovaJsfxVm() = default;

int NovaJsfxVm::getOrRegisterVariable(const std::string& name)
{
    auto it = m_varNameToIndex.find(name);
    if (it != m_varNameToIndex.end()) {
        return it->second;
    }
    int newIndex = static_cast<int>(m_variables.size());
    m_varNameToIndex[name] = newIndex;
    m_varIndexToName.push_back(name);
    m_variables.push_back(0.0);
    return newIndex;
}

double NovaJsfxVm::getVariable(int index) const
{
    if (index >= 0 && index < static_cast<int>(m_variables.size())) {
        return m_variables[static_cast<size_t>(index)];
    }
    return 0.0;
}

void NovaJsfxVm::setVariable(int index, double value)
{
    if (index >= 0 && index < static_cast<int>(m_variables.size())) {
        m_variables[static_cast<size_t>(index)] = value;
    }
}

double NovaJsfxVm::getVariableByName(const std::string& name) const
{
    auto it = m_varNameToIndex.find(name);
    if (it != m_varNameToIndex.end()) {
        return m_variables[static_cast<size_t>(it->second)];
    }
    return 0.0;
}

void NovaJsfxVm::setVariableByName(const std::string& name, double value)
{
    auto it = m_varNameToIndex.find(name);
    if (it != m_varNameToIndex.end()) {
        m_variables[static_cast<size_t>(it->second)] = value;
    } else {
        int idx = const_cast<NovaJsfxVm*>(this)->getOrRegisterVariable(name);
        m_variables[static_cast<size_t>(idx)] = value;
    }
}

double NovaJsfxVm::readMem(size_t index) const
{
    if (index < m_ram.size()) {
        return m_ram[index];
    }
    return 0.0;
}

void NovaJsfxVm::writeMem(size_t index, double value)
{
    if (index >= m_ram.size()) {
        if (index > 8388608) return; // Límite de 8 MB
        m_ram.resize(std::max(index + 1024, m_ram.size() * 2), 0.0);
    }
    m_ram[index] = value;
}

void NovaJsfxVm::clearMemory()
{
    std::fill(m_ram.begin(), m_ram.end(), 0.0);
}

// ── COMPILADOR DE EXPRESIONES EEL2 ───────────────────────────────────

enum TokenType {
    TOK_EOF,
    TOK_NUM,
    TOK_IDENT,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MUL,
    TOK_DIV,
    TOK_MOD,
    TOK_POW,
    TOK_ASSIGN,
    TOK_PLUS_ASSIGN,
    TOK_MINUS_ASSIGN,
    TOK_MUL_ASSIGN,
    TOK_DIV_ASSIGN,
    TOK_EQ,
    TOK_NEQ,
    TOK_LT,
    TOK_LTE,
    TOK_GT,
    TOK_GTE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_QUESTION,
    TOK_COLON,
    TOK_COMMA,
    TOK_SEMICOLON
};

struct Token {
    TokenType type = TOK_EOF;
    std::string text;
    double numberValue = 0.0;
};

class Lexer {
public:
    explicit Lexer(const std::string& src) : m_src(src), m_pos(0) {}

    Token nextToken() {
        skipWhitespaceAndComments();
        if (m_pos >= m_src.size()) {
            return Token{TOK_EOF, "", 0.0};
        }

        char c = m_src[m_pos];

        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && m_pos + 1 < m_src.size() && std::isdigit(static_cast<unsigned char>(m_src[m_pos + 1])))) {
            size_t start = m_pos;
            while (m_pos < m_src.size() && (std::isdigit(static_cast<unsigned char>(m_src[m_pos])) || m_src[m_pos] == '.' || m_src[m_pos] == 'e' || m_src[m_pos] == 'E' || (m_src[m_pos] == '-' && (m_src[m_pos - 1] == 'e' || m_src[m_pos - 1] == 'E')))) {
                ++m_pos;
            }
            std::string numStr = m_src.substr(start, m_pos - start);
            double val = std::strtod(numStr.c_str(), nullptr);
            return Token{TOK_NUM, numStr, val};
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = m_pos;
            while (m_pos < m_src.size() && (std::isalnum(static_cast<unsigned char>(m_src[m_pos])) || m_src[m_pos] == '_' || m_src[m_pos] == '.')) {
                ++m_pos;
            }
            std::string ident = m_src.substr(start, m_pos - start);
            return Token{TOK_IDENT, ident, 0.0};
        }

        ++m_pos;
        switch (c) {
        case '+':
            if (m_pos < m_src.size() && m_src[m_pos] == '=') { ++m_pos; return Token{TOK_PLUS_ASSIGN, "+=", 0.0}; }
            return Token{TOK_PLUS, "+", 0.0};
        case '-':
            if (m_pos < m_src.size() && m_src[m_pos] == '=') { ++m_pos; return Token{TOK_MINUS_ASSIGN, "-=", 0.0}; }
            return Token{TOK_MINUS, "-", 0.0};
        case '*':
            if (m_pos < m_src.size() && m_src[m_pos] == '=') { ++m_pos; return Token{TOK_MUL_ASSIGN, "*=", 0.0}; }
            return Token{TOK_MUL, "*", 0.0};
        case '/':
            if (m_pos < m_src.size() && m_src[m_pos] == '=') { ++m_pos; return Token{TOK_DIV_ASSIGN, "/=", 0.0}; }
            return Token{TOK_DIV, "/", 0.0};
        case '%': return Token{TOK_MOD, "%", 0.0};
        case '^': return Token{TOK_POW, "^", 0.0};
        case '=':
            if (m_pos < m_src.size() && m_src[m_pos] == '=') { ++m_pos; return Token{TOK_EQ, "==", 0.0}; }
            return Token{TOK_ASSIGN, "=", 0.0};
        case '!':
            if (m_pos < m_src.size() && m_src[m_pos] == '=') { ++m_pos; return Token{TOK_NEQ, "!=", 0.0}; }
            return Token{TOK_NOT, "!", 0.0};
        case '<':
            if (m_pos < m_src.size() && m_src[m_pos] == '=') { ++m_pos; return Token{TOK_LTE, "<=", 0.0}; }
            return Token{TOK_LT, "<", 0.0};
        case '>':
            if (m_pos < m_src.size() && m_src[m_pos] == '=') { ++m_pos; return Token{TOK_GTE, ">=", 0.0}; }
            return Token{TOK_GT, ">", 0.0};
        case '&':
            if (m_pos < m_src.size() && m_src[m_pos] == '&') { ++m_pos; return Token{TOK_AND, "&&", 0.0}; }
            return Token{TOK_AND, "&", 0.0};
        case '|':
            if (m_pos < m_src.size() && m_src[m_pos] == '|') { ++m_pos; return Token{TOK_OR, "||", 0.0}; }
            return Token{TOK_OR, "|", 0.0};
        case '(': return Token{TOK_LPAREN, "(", 0.0};
        case ')': return Token{TOK_RPAREN, ")", 0.0};
        case '[': return Token{TOK_LBRACKET, "[", 0.0};
        case ']': return Token{TOK_RBRACKET, "]", 0.0};
        case '?': return Token{TOK_QUESTION, "?", 0.0};
        case ':': return Token{TOK_COLON, ":", 0.0};
        case ',': return Token{TOK_COMMA, ",", 0.0};
        case ';': return Token{TOK_SEMICOLON, ";", 0.0};
        default: break;
        }

        return Token{TOK_EOF, "", 0.0};
    }

private:
    void skipWhitespaceAndComments() {
        while (m_pos < m_src.size()) {
            char c = m_src[m_pos];
            if (std::isspace(static_cast<unsigned char>(c))) {
                ++m_pos;
                continue;
            }
            if (c == '/' && m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '/') {
                // Comentario de línea //
                m_pos += 2;
                while (m_pos < m_src.size() && m_src[m_pos] != '\n') {
                    ++m_pos;
                }
                continue;
            }
            if (c == '/' && m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '*') {
                // Comentario de bloque /* */
                m_pos += 2;
                while (m_pos + 1 < m_src.size() && !(m_src[m_pos] == '*' && m_src[m_pos + 1] == '/')) {
                    ++m_pos;
                }
                if (m_pos + 1 < m_src.size()) m_pos += 2;
                continue;
            }
            break;
        }
    }

public:
    size_t pos() const { return m_pos; }
    void setPos(size_t p) { m_pos = p; }

private:
    const std::string& m_src;
    size_t m_pos;
};

class Parser {
public:
    Parser(NovaJsfxVm& vm, const std::string& src) : m_vm(vm), m_lexer(src) {
        advance();
    }

    struct Checkpoint {
        size_t pos;
        Token current;
    };

    Checkpoint save() {
        return {m_lexer.pos(), m_current};
    }

    void restore(const Checkpoint& cp) {
        m_lexer.setPos(cp.pos);
        m_current = cp.current;
    }

    bool parseProgram(NovaJsfxVm::Program& outProg) {
        while (m_current.type != TOK_EOF) {
            if (m_current.type == TOK_SEMICOLON) {
                advance();
                continue;
            }
            if (!parseStatement(outProg)) {
                return false;
            }
        }
        return true;
    }

private:
    void advance() {
        m_current = m_lexer.nextToken();
    }

    bool match(TokenType type) {
        if (m_current.type == type) {
            advance();
            return true;
        }
        return false;
    }

    bool parseStatement(NovaJsfxVm::Program& outProg) {
        // En EEL2, las sentencias son expresiones evaluadas en serie
        if (!parseExpression(outProg)) {
            return false;
        }
        // Descartar el resultado en la pila al terminar la sentencia
        outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_POP, 0.0, -1, -1, 0});
        match(TOK_SEMICOLON);
        return true;
    }

    bool parseExpression(NovaJsfxVm::Program& outProg) {
        return parseAssignment(outProg);
    }

    bool parseAssignment(NovaJsfxVm::Program& outProg) {
        if (m_current.type == TOK_IDENT) {
            Checkpoint cp = save();
            std::string varName = m_current.text;
            advance();

            // Caso 1: Asignación a variable simple (var = expr, var += expr, ...)
            if (m_current.type == TOK_ASSIGN || m_current.type == TOK_PLUS_ASSIGN ||
                m_current.type == TOK_MINUS_ASSIGN || m_current.type == TOK_MUL_ASSIGN ||
                m_current.type == TOK_DIV_ASSIGN) {
                TokenType op = m_current.type;
                advance();
                int varIdx = m_vm.getOrRegisterVariable(varName);

                if (op == TOK_ASSIGN) {
                    if (!parseExpression(outProg)) return false;
                    outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_STORE_VAR, 0.0, varIdx, -1, 0});
                    return true;
                } else {
                    outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_LOAD_VAR, 0.0, varIdx, -1, 0});
                    if (!parseExpression(outProg)) return false;
                    if (op == TOK_PLUS_ASSIGN) outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_ADD, 0.0, -1, -1, 0});
                    else if (op == TOK_MINUS_ASSIGN) outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_SUB, 0.0, -1, -1, 0});
                    else if (op == TOK_MUL_ASSIGN) outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_MUL, 0.0, -1, -1, 0});
                    else if (op == TOK_DIV_ASSIGN) outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_DIV, 0.0, -1, -1, 0});
                    outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_STORE_VAR, 0.0, varIdx, -1, 0});
                    return true;
                }
            }

            // Caso 2: Asignación a memoria indexada (var[idx] = expr)
            if (m_current.type == TOK_LBRACKET) {
                size_t oldProgSize = outProg.size();
                advance();
                int baseVar = m_vm.getOrRegisterVariable(varName);
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_LOAD_VAR, 0.0, baseVar, -1, 0});
                if (parseExpression(outProg)) {
                    outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_ADD, 0.0, -1, -1, 0});
                    if (match(TOK_RBRACKET) && match(TOK_ASSIGN)) {
                        if (parseExpression(outProg)) {
                            outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_STORE_MEM, 0.0, -1, -1, 0});
                            return true;
                        }
                    }
                }
                outProg.resize(oldProgSize);
            }

            // No era asignación: restaurar estado y continuar como expresión normal
            restore(cp);
        }

        // Caso 3: Asignación a memoria con base entre paréntesis ((expr)[idx] = valor).
        // EEL2 permite calcular la dirección base con cualquier expresión, no solo
        // con un identificador: es la forma habitual de escribir delays y reverbs.
        if (m_current.type == TOK_LPAREN) {
            Checkpoint cp = save();
            size_t oldProgSize = outProg.size();
            advance(); // consumir '('

            // La expresión base ya deja su valor en la pila: sirve como dirección.
            if (parseExpression(outProg) && match(TOK_RPAREN) && m_current.type == TOK_LBRACKET) {
                advance(); // consumir '['

                if (parseExpression(outProg)) {
                    outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_ADD, 0.0, -1, -1, 0});

                    if (match(TOK_RBRACKET) && match(TOK_ASSIGN)) {
                        if (parseExpression(outProg)) {
                            outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_STORE_MEM, 0.0, -1, -1, 0});
                            return true;
                        }
                    }
                }
            }

            outProg.resize(oldProgSize);
            restore(cp);
        }

        return parseTernary(outProg);
    }

    bool parseTernary(NovaJsfxVm::Program& outProg) {
        if (!parseLogicalOr(outProg)) return false;
        return parseTernaryTail(outProg);
    }

    bool parseTernaryTail(NovaJsfxVm::Program& outProg) {
        if (match(TOK_QUESTION)) {
            // Salto condicional
            size_t jumpIfZeroIdx = outProg.size();
            outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_JUMP_IF_ZERO, 0.0, -1, -1, 0});

            if (!parseExpression(outProg)) return false;

            size_t jumpToEndIdx = outProg.size();
            outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_JUMP, 0.0, -1, -1, 0});

            if (!match(TOK_COLON)) return false;

            outProg[jumpIfZeroIdx].targetIp = static_cast<int>(outProg.size());

            if (!parseExpression(outProg)) return false;

            outProg[jumpToEndIdx].targetIp = static_cast<int>(outProg.size());
        }
        return true;
    }

    bool parseLogicalOr(NovaJsfxVm::Program& outProg) {
        if (!parseLogicalAnd(outProg)) return false;
        while (m_current.type == TOK_OR) {
            advance();
            if (!parseLogicalAnd(outProg)) return false;
            outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_OR, 0.0, -1, -1, 0});
        }
        return true;
    }

    bool parseLogicalAnd(NovaJsfxVm::Program& outProg) {
        if (!parseEquality(outProg)) return false;
        while (m_current.type == TOK_AND) {
            advance();
            if (!parseEquality(outProg)) return false;
            outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_AND, 0.0, -1, -1, 0});
        }
        return true;
    }

    bool parseEquality(NovaJsfxVm::Program& outProg) {
        if (!parseRelational(outProg)) return false;
        while (m_current.type == TOK_EQ || m_current.type == TOK_NEQ) {
            TokenType op = m_current.type;
            advance();
            if (!parseRelational(outProg)) return false;
            if (op == TOK_EQ) {
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_EQ, 0.0, -1, -1, 0});
            } else {
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_NEQ, 0.0, -1, -1, 0});
            }
        }
        return true;
    }

    bool parseRelational(NovaJsfxVm::Program& outProg) {
        if (!parseAdditive(outProg)) return false;
        while (m_current.type == TOK_LT || m_current.type == TOK_LTE ||
               m_current.type == TOK_GT || m_current.type == TOK_GTE) {
            TokenType op = m_current.type;
            advance();
            if (!parseAdditive(outProg)) return false;
            switch (op) {
            case TOK_LT: outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_LT, 0.0, -1, -1, 0}); break;
            case TOK_LTE: outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_LTE, 0.0, -1, -1, 0}); break;
            case TOK_GT: outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_GT, 0.0, -1, -1, 0}); break;
            case TOK_GTE: outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_GTE, 0.0, -1, -1, 0}); break;
            default: break;
            }
        }
        return true;
    }

    bool parseAdditive(NovaJsfxVm::Program& outProg) {
        if (!parseMultiplicative(outProg)) return false;
        while (m_current.type == TOK_PLUS || m_current.type == TOK_MINUS) {
            TokenType op = m_current.type;
            advance();
            if (!parseMultiplicative(outProg)) return false;
            if (op == TOK_PLUS) {
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_ADD, 0.0, -1, -1, 0});
            } else {
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_SUB, 0.0, -1, -1, 0});
            }
        }
        return true;
    }

    bool parseMultiplicative(NovaJsfxVm::Program& outProg) {
        if (!parsePower(outProg)) return false;
        while (m_current.type == TOK_MUL || m_current.type == TOK_DIV || m_current.type == TOK_MOD) {
            TokenType op = m_current.type;
            advance();
            if (!parsePower(outProg)) return false;
            if (op == TOK_MUL) {
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_MUL, 0.0, -1, -1, 0});
            } else if (op == TOK_DIV) {
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_DIV, 0.0, -1, -1, 0});
            } else {
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_MOD, 0.0, -1, -1, 0});
            }
        }
        return true;
    }

    bool parsePower(NovaJsfxVm::Program& outProg) {
        if (!parseUnary(outProg)) return false;
        if (m_current.type == TOK_POW) {
            advance();
            if (!parsePower(outProg)) return false; // Exponente asociativo a derecha
            outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_POW, 0.0, -1, -1, 0});
        }
        return true;
    }

    bool parseUnary(NovaJsfxVm::Program& outProg) {
        if (m_current.type == TOK_MINUS) {
            advance();
            if (!parseUnary(outProg)) return false;
            outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_NEG, 0.0, -1, -1, 0});
            return true;
        }
        if (m_current.type == TOK_NOT) {
            advance();
            if (!parseUnary(outProg)) return false;
            outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_NOT, 0.0, -1, -1, 0});
            return true;
        }
        if (m_current.type == TOK_PLUS) {
            advance();
            return parseUnary(outProg);
        }
        return parsePrimary(outProg);
    }

    bool parsePrimary(NovaJsfxVm::Program& outProg) {
        if (m_current.type == TOK_NUM) {
            outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_PUSH_CONST, m_current.numberValue, -1, -1, 0});
            advance();
            return true;
        }

        if (m_current.type == TOK_IDENT) {
            std::string ident = m_current.text;
            advance();

            // Caso de llamada a función: ident(arg1, arg2...)
            if (m_current.type == TOK_LPAREN) {
                advance();
                int funcId = getMathFuncId(ident);
                int argCount = 0;
                if (m_current.type != TOK_RPAREN) {
                    if (!parseExpression(outProg)) return false;
                    ++argCount;
                    while (match(TOK_COMMA)) {
                        if (!parseExpression(outProg)) return false;
                        ++argCount;
                    }
                }
                if (!match(TOK_RPAREN)) return false;
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_CALL_FUNC, static_cast<double>(argCount), -1, -1, funcId});
                return true;
            }

            // Caso de memoria: ident[expr]
            if (m_current.type == TOK_LBRACKET) {
                advance();
                int baseVar = m_vm.getOrRegisterVariable(ident);
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_LOAD_VAR, 0.0, baseVar, -1, 0});
                if (!parseExpression(outProg)) return false;
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_ADD, 0.0, -1, -1, 0});
                if (!match(TOK_RBRACKET)) return false;
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_LOAD_MEM, 0.0, -1, -1, 0});
                return true;
            }

            // Variable común
            int varIdx = m_vm.getOrRegisterVariable(ident);
            outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_LOAD_VAR, 0.0, varIdx, -1, 0});
            return true;
        }

        if (m_current.type == TOK_LPAREN) {
            advance();
            if (!parseExpression(outProg)) return false;
            if (!match(TOK_RPAREN)) return false;

            // Memoria con base entre paréntesis: (expr)[idx]
            if (m_current.type == TOK_LBRACKET) {
                advance();
                if (!parseExpression(outProg)) return false;
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_ADD, 0.0, -1, -1, 0});
                if (!match(TOK_RBRACKET)) return false;
                outProg.push_back(NovaJsfxVm::Instruction{NovaJsfxVm::OP_LOAD_MEM, 0.0, -1, -1, 0});
            }
            return true;
        }

        return false;
    }

    NovaJsfxVm& m_vm;
    Lexer m_lexer;
    Token m_current;
};

bool NovaJsfxVm::compile(const std::string& source, Program& outProgram, std::string* outError)
{
    outProgram.clear();
    if (source.empty()) return true;

    Parser parser(*this, source);
    if (!parser.parseProgram(outProgram)) {
        if (outError) *outError = "Error sintáctico al compilar script JSFX/EEL2";
        return false;
    }
    return true;
}

double NovaJsfxVm::callMathFunc(int funcId, double arg1, double arg2)
{
    switch (funcId) {
    case FN_SIN: return std::sin(arg1);
    case FN_COS: return std::cos(arg1);
    case FN_TAN: return std::tan(arg1);
    case FN_ASIN: return std::asin(arg1);
    case FN_ACOS: return std::acos(arg1);
    case FN_ATAN: return std::atan(arg1);
    case FN_ATAN2: return std::atan2(arg1, arg2);
    case FN_SQRT: return std::sqrt(std::max(0.0, arg1));
    case FN_POW: return std::pow(arg1, arg2);
    case FN_EXP: return std::exp(arg1);
    case FN_LOG: return std::log(std::max(1e-12, arg1));
    case FN_LOG10: return std::log10(std::max(1e-12, arg1));
    case FN_ABS: return std::abs(arg1);
    case FN_MIN: return std::min(arg1, arg2);
    case FN_MAX: return std::max(arg1, arg2);
    case FN_FLOOR: return std::floor(arg1);
    case FN_CEIL: return std::ceil(arg1);
    case FN_SIGN: return (arg1 > 0.0) ? 1.0 : ((arg1 < 0.0) ? -1.0 : 0.0);
    case FN_TANH: return std::tanh(arg1);
    case FN_RAND: return static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX) * arg1;
    default: return 0.0;
    }
}

void NovaJsfxVm::execute(const Program& program)
{
    if (program.empty()) return;

    m_stack.clear();
    const size_t progSize = program.size();
    size_t ip = 0;

    auto pop = [this]() -> double {
        if (m_stack.empty()) return 0.0;
        double v = m_stack.back();
        m_stack.pop_back();
        return v;
    };

    while (ip < progSize) {
        const auto& inst = program[ip++];
        switch (inst.op) {
        case OP_NOP:
            break;
        case OP_PUSH_CONST:
            m_stack.push_back(inst.value);
            break;
        case OP_LOAD_VAR:
            m_stack.push_back(getVariable(inst.varIndex));
            break;
        case OP_STORE_VAR: {
            double v = m_stack.empty() ? 0.0 : m_stack.back();
            setVariable(inst.varIndex, v);
            break;
        }
        case OP_LOAD_MEM: {
            double addr = pop();
            m_stack.push_back(readMem(static_cast<size_t>(std::max(0.0, addr))));
            break;
        }
        case OP_STORE_MEM: {
            double val = pop();
            double addr = pop();
            writeMem(static_cast<size_t>(std::max(0.0, addr)), val);
            m_stack.push_back(val);
            break;
        }
        case OP_ADD: {
            double b = pop();
            double a = pop();
            m_stack.push_back(a + b);
            break;
        }
        case OP_SUB: {
            double b = pop();
            double a = pop();
            m_stack.push_back(a - b);
            break;
        }
        case OP_MUL: {
            double b = pop();
            double a = pop();
            m_stack.push_back(a * b);
            break;
        }
        case OP_DIV: {
            double b = pop();
            double a = pop();
            m_stack.push_back((std::abs(b) > 1e-12) ? (a / b) : 0.0);
            break;
        }
        case OP_MOD: {
            double b = pop();
            double a = pop();
            m_stack.push_back((std::abs(b) > 1e-12) ? std::fmod(a, b) : 0.0);
            break;
        }
        case OP_POW: {
            double b = pop();
            double a = pop();
            m_stack.push_back(std::pow(a, b));
            break;
        }
        case OP_NEG: {
            double a = pop();
            m_stack.push_back(-a);
            break;
        }
        case OP_EQ: {
            double b = pop();
            double a = pop();
            m_stack.push_back((std::abs(a - b) < 1e-9) ? 1.0 : 0.0);
            break;
        }
        case OP_NEQ: {
            double b = pop();
            double a = pop();
            m_stack.push_back((std::abs(a - b) >= 1e-9) ? 1.0 : 0.0);
            break;
        }
        case OP_LT: {
            double b = pop();
            double a = pop();
            m_stack.push_back((a < b) ? 1.0 : 0.0);
            break;
        }
        case OP_LTE: {
            double b = pop();
            double a = pop();
            m_stack.push_back((a <= b) ? 1.0 : 0.0);
            break;
        }
        case OP_GT: {
            double b = pop();
            double a = pop();
            m_stack.push_back((a > b) ? 1.0 : 0.0);
            break;
        }
        case OP_GTE: {
            double b = pop();
            double a = pop();
            m_stack.push_back((a >= b) ? 1.0 : 0.0);
            break;
        }
        case OP_AND: {
            double b = pop();
            double a = pop();
            m_stack.push_back((std::abs(a) > 0.0 && std::abs(b) > 0.0) ? 1.0 : 0.0);
            break;
        }
        case OP_OR: {
            double b = pop();
            double a = pop();
            m_stack.push_back((std::abs(a) > 0.0 || std::abs(b) > 0.0) ? 1.0 : 0.0);
            break;
        }
        case OP_NOT: {
            double a = pop();
            m_stack.push_back((std::abs(a) == 0.0) ? 1.0 : 0.0);
            break;
        }
        case OP_CALL_FUNC: {
            int argCount = static_cast<int>(inst.value);
            if (argCount == 2) {
                double a2 = pop();
                double a1 = pop();
                m_stack.push_back(callMathFunc(inst.funcId, a1, a2));
            } else if (argCount == 1) {
                double a1 = pop();
                m_stack.push_back(callMathFunc(inst.funcId, a1));
            } else {
                m_stack.push_back(callMathFunc(inst.funcId, 0.0));
            }
            break;
        }
        case OP_JUMP_IF_ZERO: {
            double cond = pop();
            if (std::abs(cond) == 0.0 && inst.targetIp >= 0) {
                ip = static_cast<size_t>(inst.targetIp);
            }
            break;
        }
        case OP_JUMP: {
            if (inst.targetIp >= 0) {
                ip = static_cast<size_t>(inst.targetIp);
            }
            break;
        }
        case OP_POP: {
            pop();
            break;
        }
        }
    }
}
