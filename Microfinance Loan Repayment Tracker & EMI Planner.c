
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define FILE_NAME "loans.dat"
#define EPS 0.001

typedef struct {
    int day, month, year;
} Date;

typedef struct {
    Date date;
    double amount, penalty;
} Payment;

typedef struct {
    int id;
    char name[60], phone[20];
    double principal, rate, balance;
    double emi, penaltyDue;
    int tenure, remaining, paidCount;
    int dueDay, dueMonth, dueYear;
    int capacity;
    Payment *history;
} Loan;

Loan *loans = NULL;
int count = 0, capacity = 0;

Date today() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    Date d = {t->tm_mday, t->tm_mon + 1,
              t->tm_year + 1900};
    return d;
}

time_t toTime(Date d) {
    struct tm t = {0};
    t.tm_mday = d.day;
    t.tm_mon = d.month - 1;
    t.tm_year = d.year - 1900;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    return mktime(&t);
}

Date addMonths(Date d, int n) {
    struct tm t = {0};
    t.tm_mday = 1;
    t.tm_mon = d.month - 1 + n;
    t.tm_year = d.year - 1900;
    t.tm_hour = 12;
    mktime(&t);

    int year = t.tm_year + 1900;
    int month = t.tm_mon + 1;
    int day = d.day;

    struct tm next = {0};
    next.tm_year = year - 1900;
    next.tm_mon = month;
    next.tm_mday = 0;
    next.tm_hour = 12;
    mktime(&next);

    if (day > next.tm_mday)
        day = next.tm_mday;

    Date result = {day, month, year};
    return result;
}

Date dueDate(Loan *l) {
    Date d = {l->dueDay, l->dueMonth, l->dueYear};
    return d;
}

void setDue(Loan *l, Date d) {
    l->dueDay = d.day;
    l->dueMonth = d.month;
    l->dueYear = d.year;
}

int daysLate(Date d) {
    double days = difftime(toTime(today()), toTime(d));
    return days > 0 ? (int)(days / 86400) : 0;
}

double calculateEMI(double p, double annual, int n) {
    if (n <= 0 || p <= 0) return 0;

    double r = annual / 1200.0;
    if (r == 0) return p / n;

    double x = pow(1 + r, n);
    return p * r * x / (x - 1);
}

void expandLoans() {
    if (count < capacity) return;

    int newCap = capacity ? capacity * 2 : 5;
    Loan *temp = realloc(loans, newCap * sizeof(Loan));

    if (!temp) {
        printf("Memory allocation failed!\n");
        exit(1);
    }

    loans = temp;
    capacity = newCap;
}

Loan *findLoan(int id) {
    for (int i = 0; i < count; i++)
        if (loans[i].id == id)
            return &loans[i];
    return NULL;
}

void saveData() {
    FILE *fp = fopen(FILE_NAME, "wb");
    if (!fp) {
        printf("Error saving data!\n");
        return;
    }

    fwrite(&count, sizeof(int), 1, fp);

    for (int i = 0; i < count; i++) {
        Loan temp = loans[i];
        temp.history = NULL;

        fwrite(&temp, sizeof(Loan), 1, fp);
        fwrite(loans[i].history, sizeof(Payment),
               loans[i].paidCount, fp);
    }
    fclose(fp);
}

void loadData() {
    FILE *fp = fopen(FILE_NAME, "rb");
    if (!fp) return;

    int total;
    if (fread(&total, sizeof(int), 1, fp) != 1 ||
        total < 0 || total > 100000) {
        fclose(fp);
        return;
    }

    for (int i = 0; i < total; i++) {
        Loan l;

        if (fread(&l, sizeof(Loan), 1, fp) != 1)
            break;

        if (l.paidCount < 0 || l.paidCount > 100000)
            break;

        l.capacity = l.paidCount > 5 ? l.paidCount : 5;
        l.history = calloc(l.capacity, sizeof(Payment));

        if (!l.history) break;

        if (fread(l.history, sizeof(Payment),
                  l.paidCount, fp) != (size_t)l.paidCount) {
            free(l.history);
            break;
        }

        expandLoans();
        loans[count++] = l;
    }
    fclose(fp);
}

void addBorrower() {
    Loan l = {0};

    printf("\n--- NEW BORROWER ---\n");
    printf("Borrower ID: ");
    if (scanf("%d", &l.id) != 1) return;

    if (l.id <= 0 || findLoan(l.id)) {
        printf("Invalid or duplicate ID!\n");
        return;
    }

    printf("Borrower Name: ");
    scanf(" %59[^\n]", l.name);

    printf("Phone Number: ");
    scanf(" %19s", l.phone);

    printf("Loan Principal: ");
    if (scanf("%lf", &l.principal) != 1) return;

    printf("Annual Interest Rate (%%): ");
    if (scanf("%lf", &l.rate) != 1) return;

    printf("Tenure (months): ");
    if (scanf("%d", &l.tenure) != 1) return;

    if (l.principal <= 0 || l.rate < 0 ||
        l.rate > 100 || l.tenure <= 0 ||
        l.tenure > 600) {
        printf("Invalid loan details!\n");
        return;
    }

    l.balance = l.principal;
    l.remaining = l.tenure;
    l.emi = calculateEMI(l.balance, l.rate, l.remaining);

    l.capacity = 5;
    l.history = calloc(l.capacity, sizeof(Payment));

    if (!l.history) {
        printf("Memory allocation failed!\n");
        return;
    }

    setDue(&l, addMonths(today(), 1));

    expandLoans();
    loans[count++] = l;
    saveData();

    printf("\nLoan created successfully!\n");
    printf("Monthly EMI: Rs. %.2f\n", l.emi);
    printf("First Due: %02d-%02d-%04d\n",
           l.dueDay, l.dueMonth, l.dueYear);
}

void showSchedule(Loan *l) {
    double balance = l->balance;
    double r = l->rate / 1200.0;
    Date due = dueDate(l);

    printf("\n%-6s %-13s %-12s %-12s %-12s\n",
           "Month", "EMI", "Interest", "Principal", "Balance");

    for (int i = 1; i <= l->remaining; i++) {
        double interest = balance * r;
        double principal = l->emi - interest;
        double installment = l->emi;

        if (principal > balance || i == l->remaining) {
            principal = balance;
            installment = principal + interest;
        }

        balance -= principal;
        if (balance < EPS) balance = 0;

        printf("%-6d %-13.2f %-12.2f %-12.2f %-12.2f\n",
               i, installment, interest, principal, balance);

        due = addMonths(due, 1);
    }
}

void viewLoan() {
    int id;
    printf("Enter Borrower ID: ");
    if (scanf("%d", &id) != 1) return;

    Loan *l = findLoan(id);
    if (!l) {
        printf("Borrower not found!\n");
        return;
    }

    printf("\n--- LOAN DETAILS ---\n");
    printf("ID: %d\nName: %s\nPhone: %s\n",
           l->id, l->name, l->phone);
    printf("Principal: Rs. %.2f\n", l->principal);
    printf("Interest Rate: %.2f%%\n", l->rate);
    printf("Outstanding: Rs. %.2f\n", l->balance);
    printf("Monthly EMI: Rs. %.2f\n", l->emi);
    printf("Remaining Months: %d\n", l->remaining);
    printf("Penalty Due: Rs. %.2f\n", l->penaltyDue);
    printf("Next Due: %02d-%02d-%04d\n",
           l->dueDay, l->dueMonth, l->dueYear);

    if (l->balance > EPS)
        showSchedule(l);
}

void recordPayment() {
    int id;
    double amount;

    printf("\nBorrower ID: ");
    if (scanf("%d", &id) != 1) return;

    Loan *l = findLoan(id);
    if (!l || l->balance <= EPS) {
        printf("No active loan found!\n");
        return;
    }

    Date d = dueDate(l);
    int late = daysLate(d);

    /* Simplified penalty: 0.1% of EMI per late day */
    double penalty = late * l->emi * 0.001;
    l->penaltyDue += penalty;

    double interest = l->balance * l->rate / 1200.0;
    double total = l->balance + interest + l->penaltyDue;

    printf("Days Overdue: %d\n", late);
    printf("New Penalty: Rs. %.2f\n", penalty);
    printf("Total Payoff Due: Rs. %.2f\n", total);
    printf("Payment Amount: Rs. ");

    if (scanf("%lf", &amount) != 1) return;

    if (amount <= 0 || amount > total + EPS) {
        printf("Invalid payment amount!\n");
        return;
    }

    double original = amount;
    double penaltyPaid = fmin(amount, l->penaltyDue);
    amount -= penaltyPaid;
    l->penaltyDue -= penaltyPaid;

    double interestPaid = fmin(amount, interest);
    amount -= interestPaid;

    double principalPaid = fmin(amount, l->balance);
    l->balance -= principalPaid;

    if (l->balance < EPS) l->balance = 0;

    if (l->paidCount == l->capacity) {
        int newCap = l->capacity * 2;
        Payment *temp = realloc(l->history,
                                newCap * sizeof(Payment));
        if (!temp) {
            printf("History memory error!\n");
            return;
        }
        l->history = temp;
        l->capacity = newCap;
    }

    l->history[l->paidCount++] =
        (Payment){today(), original, penaltyPaid};

    if (l->balance == 0 && l->penaltyDue < EPS) {
        l->remaining = 0;
        l->emi = 0;
        printf("Loan fully repaid!\n");
    } else {
        if (l->remaining > 1)
            l->remaining--;

        l->emi = calculateEMI(l->balance,
                              l->rate, l->remaining);
        setDue(l, addMonths(d, 1));
        printf("Updated EMI: Rs. %.2f\n", l->emi);
    }

    saveData();
    printf("Payment recorded successfully!\n");
}

void paymentHistory() {
    int id;
    printf("Borrower ID: ");
    if (scanf("%d", &id) != 1) return;

    Loan *l = findLoan(id);
    if (!l) {
        printf("Borrower not found!\n");
        return;
    }

    printf("\n--- PAYMENT HISTORY: %s ---\n", l->name);

    if (!l->paidCount) {
        printf("No payments recorded.\n");
        return;
    }

    for (int i = 0; i < l->paidCount; i++) {
        Payment p = l->history[i];
        printf("%d. %02d-%02d-%04d | Rs. %.2f | Penalty Rs. %.2f\n",
               i + 1, p.date.day, p.date.month,
               p.date.year, p.amount, p.penalty);
    }
}

double riskScore(Loan *l) {
    if (l->balance <= EPS) return 0;

    int late = daysLate(dueDate(l));
    double score = late * 2.0 + l->penaltyDue * 0.01;

    if (l->paidCount == 0) score += 10;
    if (l->balance > l->principal * 0.8)
        score += 10;

    return fmin(100, score);
}

void dashboard() {
    double outstanding = 0, collected = 0;
    int active = 0, overdue = 0;

    for (int i = 0; i < count; i++) {
        Loan *l = &loans[i];

        outstanding += l->balance + l->penaltyDue;

        for (int j = 0; j < l->paidCount; j++)
            collected += l->history[j].amount;

        if (l->balance > EPS) {
            active++;
            if (daysLate(dueDate(l)) > 0)
                overdue++;
        }
    }

    printf("\n========== PORTFOLIO DASHBOARD ==========\n");
    printf("Total Borrowers    : %d\n", count);
    printf("Active Loans       : %d\n", active);
    printf("Total Outstanding  : Rs. %.2f\n", outstanding);
    printf("Total Collections  : Rs. %.2f\n", collected);
    printf("Overdue Loans      : %d\n", overdue);
    printf("Delinquency Rate   : %.2f%%\n",
           active ? overdue * 100.0 / active : 0);
}

void riskRanking() {
    if (!count) {
        printf("No borrowers available.\n");
        return;
    }

    int *index = malloc(count * sizeof(int));
    if (!index) return;

    for (int i = 0; i < count; i++)
        index[i] = i;

    for (int i = 0; i < count - 1; i++)
        for (int j = i + 1; j < count; j++)
            if (riskScore(&loans[index[j]]) >
                riskScore(&loans[index[i]])) {
                int temp = index[i];
                index[i] = index[j];
                index[j] = temp;
            }

    printf("\n--- BORROWER RISK RANKING ---\n");
    printf("%-6s %-22s %-12s %-10s\n",
           "ID", "Name", "Risk Score", "Category");

    for (int i = 0; i < count; i++) {
        Loan *l = &loans[index[i]];
        double score = riskScore(l);

        printf("%-6d %-22s %-12.1f %-10s\n",
               l->id, l->name, score,
               score >= 60 ? "HIGH" :
               score >= 30 ? "MEDIUM" : "LOW");
    }
    free(index);
}

int main() {
    int choice;
    loadData();

    do {
        printf("\n========== MICROFINANCE TRACKER ==========\n");
        printf("1. Add Borrower & Loan\n");
        printf("2. View Loan & EMI Schedule\n");
        printf("3. Record Repayment\n");
        printf("4. Repayment History\n");
        printf("5. Portfolio Dashboard\n");
        printf("6. Borrower Risk Ranking\n");
        printf("7. Save & Exit\n");
        printf("Enter Choice: ");

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input!\n");
            break;
        }

        switch (choice) {
            case 1: addBorrower(); break;
            case 2: viewLoan(); break;
            case 3: recordPayment(); break;
            case 4: paymentHistory(); break;
            case 5: dashboard(); break;
            case 6: riskRanking(); break;
            case 7:
                saveData();
                printf("Data saved. Goodbye!\n");
                break;
            default:
                printf("Invalid choice!\n");
        }
    } while (choice != 7);

    for (int i = 0; i < count; i++)
        free(loans[i].history);

    free(loans);
    return 0;
}
