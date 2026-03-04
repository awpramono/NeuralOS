#include "system.h"

// ============================================================================
// NeuralOS Ephemeral Sandbox (Virtual Machine)
// Enables on-the-fly execution of logic without persistent compilation
// Concept: A simple Stack Machine (Reverse Polish Notation)
// ============================================================================

#define VM_STACK_SIZE 64

static int vm_stack[VM_STACK_SIZE];
static int vm_sp = 0;

static void vm_push(int val) {
    if (vm_sp < VM_STACK_SIZE) {
        vm_stack[vm_sp++] = val;
    } else {
        serial_print_string("[VM Error] Stack overflow\n");
    }
}

static int vm_pop() {
    if (vm_sp > 0) {
        return vm_stack[--vm_sp];
    } else {
        serial_print_string("[VM Error] Stack underflow\n");
        return 0;
    }
}

// Case-insensitive string compare for VM tokens
static int vm_streq_ci(const char *a, const char *b) {
    while (*a && *b) {
        char ca = (*a >= 'a' && *a <= 'z') ? *a - 32 : *a;
        char cb = (*b >= 'a' && *b <= 'z') ? *b - 32 : *b;
        if (ca != cb) return 0;
        a++; b++;
    }
    return (*a == *b);
}

// Very simple string to integer parser
static int parse_int(const char *str, int *out_val) {
    int val = 0;
    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    }
    if (*str < '0' || *str > '9') return 0; // Not a number
    while (*str >= '0' && *str <= '9') {
        val = val * 10 + (*str - '0');
        str++;
    }
    if (*str != '\0' && *str != ' ') return 0; // Partially parsed
    *out_val = val * sign;
    return 1;
}

// Executes an ephemeral script (space-separated tokens)
void vm_execute(const char *script) {
    serial_print_string("[SANDBOX] Assembling & Executing Ephemeral Core for: ");
    serial_print_string(script);
    serial_print_string("\n");
    vm_sp = 0; // Reset stack
    
    char token[32];
    int t_idx = 0;
    int i = 0;
    int ops_executed = 0;
    
    while (1) {
        // Skip spaces
        while (script[i] == ' ') i++;
        if (script[i] == '\0') break;
        
        // Read token
        t_idx = 0;
        while (script[i] != ' ' && script[i] != '\0' && t_idx < 31) {
            token[t_idx++] = script[i++];
        }
        token[t_idx] = '\0';
        
        // Process token
        int val;
        if (parse_int(token, &val)) {
            vm_push(val);
        } else if (vm_streq_ci(token, "+") || vm_streq_ci(token, "ADD")) {
            int b = vm_pop(); int a = vm_pop(); vm_push(a + b);
        } else if (vm_streq_ci(token, "-") || vm_streq_ci(token, "SUB")) {
            int b = vm_pop(); int a = vm_pop(); vm_push(a - b);
        } else if (vm_streq_ci(token, "*") || vm_streq_ci(token, "MUL")) {
            int b = vm_pop(); int a = vm_pop(); vm_push(a * b);
        } else if (vm_streq_ci(token, "/") || vm_streq_ci(token, "DIV")) {
            int b = vm_pop(); 
            if (b == 0) { serial_print_string("[VM Error] Divide by zero\n"); vm_push(0); }
            else { int a = vm_pop(); vm_push(a / b); }
        } else if (vm_streq_ci(token, "DUP")) {
            if (vm_sp > 0) vm_push(vm_stack[vm_sp - 1]);
        } else if (vm_streq_ci(token, "SWAP")) {
            if (vm_sp >= 2) {
                int temp = vm_stack[vm_sp - 1];
                vm_stack[vm_sp - 1] = vm_stack[vm_sp - 2];
                vm_stack[vm_sp - 2] = temp;
            }
        } else if (vm_streq_ci(token, "PRINT") || vm_streq_ci(token, ".") || vm_streq_ci(token, "SHOW")) {
            int a = vm_pop();
            print_string("  --> [Out] ", 0x0A);
            if (a < 0) { print_char('-', 0x0F); a = -a; }
            print_number(a, 0x0F);
            print_string("\n", 0x0A);
        } else {
            serial_print_string("[VM Error] Unknown instruction: ");
            serial_print_string(token);
            serial_print_string("\n");
            break;
        }
        ops_executed++;
    }
    
    serial_print_string("[SANDBOX] Execution terminated. Operations: ");
    serial_print_number(ops_executed);
    serial_print_string("\n");
}
