
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <limits>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

// ==========================================
// JZC PAY - GLOBAL DATA
// ==========================================

json users = json::array();
json transactions = json::array();
json billHistory = json::array();

string currentUser = "";

const string USERS_FILE = "users.json";
const string TRANSACTIONS_FILE = "transactions.json";
const string BILL_HISTORY_FILE = "bill_history.json";

// ==========================================
// FUNCTION DECLARATIONS
// ==========================================

void registerAccount();
bool login();
void dashboard();

void transferMoney();
void billPaymentMenu();
void billPaymentHistory();
void transactionHistory();
void checkBalance();
void foodPayment();
void buyLoad();

void payBill(const string& billType);
void payTuition();
void showHistory(bool billsOnly);

// ==========================================
// GENERAL HELPERS
// ==========================================

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// Automatic delay. No ENTER required.
void pauseScreen() {
    this_thread::sleep_for(chrono::milliseconds(1000));
}

void showHeader(const string& title) {
    clearScreen();

    cout << "============================================\n";
    cout << "                 JZC PAY\n";
    cout << "============================================\n";
    cout << " " << title << "\n";
    cout << "--------------------------------------------\n";
}

void showFooter() {
    cout << "\n--------------------------------------------\n";
    cout << "              Developed by jez\n";
    cout << "============================================\n";
}

string getInput(const string& prompt) {
    cout << prompt;

    string value;
    getline(cin, value);

    return value;
}

int getChoice() {
    string input;
    getline(cin, input);

    try {
        size_t position = 0;
        int choice = stoi(input, &position);

        if (position != input.length()) {
            return -1;
        }

        return choice;
    }
    catch (...) {
        return -1;
    }
}

string toLower(string value) {
    transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) {
            return static_cast<char>(tolower(c));
        }
    );

    return value;
}

string getTime() {
    time_t now = time(nullptr);
    tm* local = localtime(&now);

    if (local == nullptr) {
        return "Unknown date";
    }

    stringstream ss;
    ss << put_time(local, "%Y-%m-%d %H:%M:%S");

    return ss.str();
}

void removeLast(json& data) {
    if (!data.empty()) {
        data.erase(data.end() - 1);
    }
}

// ==========================================
// MONEY HELPERS
// Stored in centavos.
// 900000 = PHP 9,000.00
// ==========================================

string formatMoney(long long centavos) {
    bool negative = centavos < 0;

    unsigned long long amount =
        negative
        ? static_cast<unsigned long long>(-(centavos + 1)) + 1
        : static_cast<unsigned long long>(centavos);

    unsigned long long pesos = amount / 100;
    unsigned long long cents = amount % 100;

    string pesoString = to_string(pesos);

    for (int i = static_cast<int>(pesoString.length()) - 3;
         i > 0;
         i -= 3) {
        pesoString.insert(static_cast<size_t>(i), ",");
    }

    stringstream result;

    if (negative) {
        result << "-";
    }

    result << "PHP " << pesoString
           << "." << setw(2) << setfill('0') << cents;

    return result.str();
}

bool parseMoney(const string& input, long long& result) {
    if (input.empty()) {
        return false;
    }

    string value = input;

    value.erase(
        remove(value.begin(), value.end(), ','),
        value.end()
    );

    if (value.empty()) {
        return false;
    }

    size_t decimalPosition = value.find('.');

    if (decimalPosition != string::npos &&
        value.find('.', decimalPosition + 1) != string::npos) {
        return false;
    }

    string pesos = value.substr(0, decimalPosition);
    string cents = "";

    if (decimalPosition != string::npos) {
        cents = value.substr(decimalPosition + 1);
    }

    if (pesos.empty()) {
        pesos = "0";
    }

    if (cents.length() > 2) {
        return false;
    }

    if (cents.empty()) {
        cents = "00";
    }
    else if (cents.length() == 1) {
        cents += "0";
    }

    bool validPesos = all_of(
        pesos.begin(),
        pesos.end(),
        [](unsigned char c) { return isdigit(c); }
    );

    bool validCents = all_of(
        cents.begin(),
        cents.end(),
        [](unsigned char c) { return isdigit(c); }
    );

    if (!validPesos || !validCents) {
        return false;
    }

    try {
        long long pesoAmount = stoll(pesos);
        long long centAmount = stoll(cents);

        if (pesoAmount >
            (numeric_limits<long long>::max() - centAmount) / 100) {
            return false;
        }

        result = pesoAmount * 100 + centAmount;

        return result > 0;
    }
    catch (...) {
        return false;
    }
}

long long getAmount(const string& prompt) {
    while (true) {
        string input = getInput(prompt);

        long long amount = 0;

        if (parseMoney(input, amount)) {
            return amount;
        }

        cout << "\nInvalid amount. Example: 100 or 100.50\n";
    }
}

bool confirmAction() {
    string answer = toLower(
        getInput("\nConfirm transaction? (Y/N): ")
    );

    return answer == "y" || answer == "yes";
}

// ==========================================
// JSON STORAGE
// ==========================================

void loadData() {
    ifstream userFile(USERS_FILE);

    if (userFile.is_open()) {
        try {
            userFile >> users;

            if (!users.is_array()) {
                users = json::array();
            }
        }
        catch (...) {
            users = json::array();
        }
    }

    ifstream transactionFile(TRANSACTIONS_FILE);

    if (transactionFile.is_open()) {
        try {
            transactionFile >> transactions;

            if (!transactions.is_array()) {
                transactions = json::array();
            }
        }
        catch (...) {
            transactions = json::array();
        }
    }

    ifstream billFile(BILL_HISTORY_FILE);

    if (billFile.is_open()) {
        try {
            billFile >> billHistory;

            if (!billHistory.is_array()) {
                billHistory = json::array();
            }
        }
        catch (...) {
            billHistory = json::array();
        }
    }
}

bool saveData() {
    {
        ofstream file(USERS_FILE);

        if (!file.is_open()) {
            return false;
        }

        file << setw(4) << users;
        file.close();

        if (!file) {
            return false;
        }
    }

    {
        ofstream file(TRANSACTIONS_FILE);

        if (!file.is_open()) {
            return false;
        }

        file << setw(4) << transactions;
        file.close();

        if (!file) {
            return false;
        }
    }

    {
        ofstream file(BILL_HISTORY_FILE);

        if (!file.is_open()) {
            return false;
        }

        file << setw(4) << billHistory;
        file.close();

        if (!file) {
            return false;
        }
    }

    return true;
}

// ==========================================
// USER HELPERS
// ==========================================

int findUser(const string& username) {
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i].value("username", "") == username) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

int getCurrentUserIndex() {
    return findUser(currentUser);
}

bool hasEnoughBalance(long long amount) {
    int index = getCurrentUserIndex();

    if (index == -1) {
        return false;
    }

    return users[index].value("balance", 0LL) >= amount;
}

// ==========================================
// TRANSACTION RECORDS
// ==========================================

void addTransaction(
    const string& type,
    const string& description,
    long long amount
) {
    json transaction;

    transaction["username"] = currentUser;
    transaction["type"] = type;
    transaction["description"] = description;
    transaction["amount"] = amount;
    transaction["date"] = getTime();

    transactions.push_back(transaction);
}

void addBillRecord(
    const string& billType,
    const string& reference,
    long long amount
) {
    json bill;

    bill["username"] = currentUser;
    bill["bill_type"] = billType;
    bill["reference"] = reference;
    bill["amount"] = amount;
    bill["date"] = getTime();

    billHistory.push_back(bill);
}

// ==========================================
// REGISTRATION
// ==========================================

void registerAccount() {
    showHeader("CREATE ACCOUNT");

    string username = getInput("Username: ");

    if (username.empty() ||
        username.find_first_of(" \t\r\n") != string::npos) {
        cout << "\nInvalid username.\n";
        pauseScreen();
        return;
    }

    if (findUser(username) != -1) {
        cout << "\nUsername already exists.\n";
        pauseScreen();
        return;
    }

    string fullname = getInput("Full Name: ");

    if (fullname.empty()) {
        cout << "\nFull name is required.\n";
        pauseScreen();
        return;
    }

    string password = getInput("Password: ");

    if (password.length() < 4) {
        cout << "\nPassword must have at least 4 characters.\n";
        pauseScreen();
        return;
    }

    string confirmPassword = getInput("Confirm Password: ");

    if (password != confirmPassword) {
        cout << "\nPasswords do not match.\n";
        pauseScreen();
        return;
    }

    json newUser;

    newUser["username"] = username;
    newUser["fullname"] = fullname;
    newUser["password"] = password;
    newUser["balance"] = 0;

    users.push_back(newUser);

    if (!saveData()) {
        removeLast(users);

        cout << "\nUnable to save account.\n";
        pauseScreen();
        return;
    }

    cout << "\nAccount successfully created!\n";
    cout << "Welcome to JZC Pay, " << fullname << "!\n";

    pauseScreen();
}

// ==========================================
// LOGIN
// ==========================================

bool login() {
    showHeader("LOGIN");

    string username = getInput("Username (0 to cancel): ");

    if (username == "0") {
        return false;
    }

    int index = findUser(username);

    if (index == -1) {
        cout << "\nAccount not found.\n";
        pauseScreen();
        return false;
    }

    int attempts = 3;

    while (attempts > 0) {
        string password = getInput("Password (0 to cancel): ");

        if (password == "0") {
            return false;
        }

        if (password == users[index].value("password", "")) {
            currentUser = username;

            cout << "\nLogin successful!\n";
            cout << "Welcome, "
                 << users[index].value("fullname", username)
                 << "!\n";

            pauseScreen();
            return true;
        }

        attempts--;

        cout << "\nIncorrect password.\n";

        if (attempts > 0) {
            cout << "Remaining attempts: " << attempts << "\n";
        }
    }

    cout << "\nToo many failed attempts.\n";
    pauseScreen();

    return false;
}

// ==========================================
// CHECK BALANCE
// ==========================================

void checkBalance() {
    showHeader("ACCOUNT BALANCE");

    int index = getCurrentUserIndex();

    if (index == -1) {
        cout << "Account not found.\n";
        pauseScreen();
        return;
    }

    cout << "Account Name : "
         << users[index].value("fullname", "") << "\n";

    cout << "Username     : " << currentUser << "\n";

    cout << "Balance      : "
         << formatMoney(users[index].value("balance", 0LL)) << "\n";

    showFooter();
    pauseScreen();
}

// ==========================================
// MONEY TRANSFER
// ==========================================

void transferMoney() {
    while (true) {
        showHeader("MONEY TRANSFER");

        cout << "Enter 0 to go back.\n\n";

        string recipient = getInput("Recipient Username: ");

        if (recipient == "0") {
            return;
        }

        int senderIndex = getCurrentUserIndex();
        int recipientIndex = findUser(recipient);

        if (recipient == currentUser) {
            cout << "\nCannot transfer to yourself.\n";
            pauseScreen();
            return;
        }

        if (senderIndex == -1 || recipientIndex == -1) {
            cout << "\nRecipient account not found.\n";
            pauseScreen();
            return;
        }

        long long amount = getAmount("Transfer Amount: ");

        if (users[senderIndex].value("balance", 0LL) < amount) {
            cout << "\nInsufficient balance.\n";
            pauseScreen();
            return;
        }

        long long recipientBalance =
            users[recipientIndex].value("balance", 0LL);

        if (recipientBalance >
            numeric_limits<long long>::max() - amount) {
            cout << "\nRecipient balance limit exceeded.\n";
            pauseScreen();
            return;
        }

        cout << "\nTRANSFER SUMMARY\n";
        cout << "Recipient: "
             << users[recipientIndex].value("fullname", "") << "\n";
        cout << "Amount: " << formatMoney(amount) << "\n";

        if (!confirmAction()) {
            cout << "\nTransfer cancelled.\n";
            pauseScreen();
            return;
        }

        json oldUsers = users;
        json oldTransactions = transactions;

        users[senderIndex]["balance"] =
            users[senderIndex]["balance"].get<long long>() - amount;

        users[recipientIndex]["balance"] =
            recipientBalance + amount;

        addTransaction(
            "Transfer Sent",
            "Transfer to " + recipient,
            amount
        );

        json received;

        received["username"] = recipient;
        received["type"] = "Transfer Received";
        received["description"] = "Transfer from " + currentUser;
        received["amount"] = amount;
        received["date"] = getTime();

        transactions.push_back(received);

        if (!saveData()) {
            users = oldUsers;
            transactions = oldTransactions;

            cout << "\nUnable to save transfer.\n";
            pauseScreen();
            return;
        }

        cout << "\nTransfer successful!\n";
        cout << "Amount sent: " << formatMoney(amount) << "\n";

        pauseScreen();
        return;
    }
}

// ==========================================
// REGULAR BILL PAYMENT
// ==========================================

void payBill(const string& billType) {
    showHeader(billType);

    cout << "Enter 0 to go back.\n\n";

    string reference = getInput("Account / Reference Number: ");

    if (reference == "0") {
        return;
    }

    if (reference.empty()) {
        cout << "\nReference number is required.\n";
        pauseScreen();
        return;
    }

    long long amount = getAmount("Payment Amount: ");

    if (!hasEnoughBalance(amount)) {
        cout << "\nInsufficient balance.\n";
        pauseScreen();
        return;
    }

    cout << "\nBILL PAYMENT SUMMARY\n";
    cout << "Bill Type: " << billType << "\n";
    cout << "Reference: " << reference << "\n";
    cout << "Amount: " << formatMoney(amount) << "\n";

    if (!confirmAction()) {
        cout << "\nPayment cancelled.\n";
        pauseScreen();
        return;
    }

    int index = getCurrentUserIndex();

    json oldUsers = users;
    json oldTransactions = transactions;
    json oldBillHistory = billHistory;

    users[index]["balance"] =
        users[index]["balance"].get<long long>() - amount;

    addTransaction(
        "Bill Payment",
        billType + " - Reference: " + reference,
        amount
    );

    addBillRecord(billType, reference, amount);

    if (!saveData()) {
        users = oldUsers;
        transactions = oldTransactions;
        billHistory = oldBillHistory;

        cout << "\nUnable to save payment.\n";
        pauseScreen();
        return;
    }

    cout << "\nBill payment successful!\n";
    cout << "Paid: " << formatMoney(amount) << "\n";

    pauseScreen();
}

// ==========================================
// TUITION PAYMENT
// ==========================================

void payTuition() {
    showHeader("TUITION FEE PAYMENT");

    cout << "Enter 0 to go back.\n\n";

    string studentName = getInput("Student Name: ");

    if (studentName == "0") {
        return;
    }

    string studentNumber = getInput("Student Number: ");

    if (studentNumber == "0") {
        return;
    }

    string schoolName = getInput("School Name: ");

    if (schoolName == "0") {
        return;
    }

    if (studentName.empty() ||
        studentNumber.empty() ||
        schoolName.empty()) {
        cout << "\nAll student information is required.\n";
        pauseScreen();
        return;
    }

    long long amount = getAmount("Tuition Amount: ");

    if (!hasEnoughBalance(amount)) {
        cout << "\nInsufficient balance.\n";
        pauseScreen();
        return;
    }

    string reference = studentNumber;

    cout << "\nTUITION PAYMENT SUMMARY\n";
    cout << "Student: " << studentName << "\n";
    cout << "Student Number: " << studentNumber << "\n";
    cout << "School: " << schoolName << "\n";
    cout << "Amount: " << formatMoney(amount) << "\n";

    if (!confirmAction()) {
        cout << "\nPayment cancelled.\n";
        pauseScreen();
        return;
    }

    int index = getCurrentUserIndex();

    json oldUsers = users;
    json oldTransactions = transactions;
    json oldBillHistory = billHistory;

    users[index]["balance"] =
        users[index]["balance"].get<long long>() - amount;

    addTransaction(
        "Tuition Payment",
        "Student: " + studentName +
        " | Student No: " + studentNumber +
        " | School: " + schoolName,
        amount
    );

    addBillRecord("Tuition Fee", reference, amount);

    if (!saveData()) {
        users = oldUsers;
        transactions = oldTransactions;
        billHistory = oldBillHistory;

        cout << "\nUnable to save tuition payment.\n";
        pauseScreen();
        return;
    }

    cout << "\nTuition payment successful!\n";

    pauseScreen();
}

// ==========================================
// BILL PAYMENT HISTORY
// ==========================================

void billPaymentHistory() {
    showHeader("BILL PAYMENT HISTORY");

    bool found = false;

    for (size_t i = billHistory.size(); i > 0; i--) {
        const json& bill = billHistory[i - 1];

        if (bill.value("username", "") != currentUser) {
            continue;
        }

        found = true;

        cout << "\nDate: "
             << bill.value("date", "") << "\n";

        cout << "Bill Type: "
             << bill.value("bill_type", "") << "\n";

        cout << "Reference: "
             << bill.value("reference", "") << "\n";

        cout << "Amount: "
             << formatMoney(bill.value("amount", 0LL)) << "\n";

        cout << "--------------------------------------------\n";
    }

    if (!found) {
        cout << "\nNo bill payment records found.\n";
    }

    showFooter();
    pauseScreen();
}

// ==========================================
// BILL PAYMENT MENU
// ==========================================

void billPaymentMenu() {
    while (true) {
        showHeader("PAY BILLS");

        cout << "[1] Electricity\n";
        cout << "[2] Water\n";
        cout << "[3] Internet\n";
        cout << "[4] Tuition Fee\n";
        cout << "[5] Other Bills\n";
        cout << "[6] Bill Payment History\n";
        cout << "[0] Back\n";

        showFooter();

        cout << "\nSelect option: ";

        int choice = getChoice();

        switch (choice) {
            case 1:
                payBill("Electricity");
                break;

            case 2:
                payBill("Water");
                break;

            case 3:
                payBill("Internet");
                break;

            case 4:
                payTuition();
                break;

            case 5:
                payBill("Other Bills");
                break;

            case 6:
                billPaymentHistory();
                break;

            case 0:
                return;

            default:
                cout << "\nInvalid option.\n";
                pauseScreen();
                break;
        }
    }
}

// ==========================================
// FOOD PAYMENT
// ==========================================

void foodPayment() {
    showHeader("FOOD PAYMENT");

    cout << "Enter 0 to go back.\n\n";

    string merchant = getInput("Restaurant / Merchant Name: ");

    if (merchant == "0") {
        return;
    }

    if (merchant.empty()) {
        cout << "\nMerchant name is required.\n";
        pauseScreen();
        return;
    }

    string order = getInput("Order Description: ");

    if (order == "0") {
        return;
    }

    if (order.empty()) {
        cout << "\nOrder description is required.\n";
        pauseScreen();
        return;
    }

    long long amount = getAmount("Total Amount: ");

    if (!hasEnoughBalance(amount)) {
        cout << "\nInsufficient balance.\n";
        pauseScreen();
        return;
    }

    cout << "\nFOOD PAYMENT SUMMARY\n";
    cout << "Merchant: " << merchant << "\n";
    cout << "Order: " << order << "\n";
    cout << "Amount: " << formatMoney(amount) << "\n";

    if (!confirmAction()) {
        cout << "\nPayment cancelled.\n";
        pauseScreen();
        return;
    }

    int index = getCurrentUserIndex();

    json oldUsers = users;
    json oldTransactions = transactions;

    users[index]["balance"] =
        users[index]["balance"].get<long long>() - amount;

    addTransaction(
        "Food Payment",
        merchant + " - " + order,
        amount
    );

    if (!saveData()) {
        users = oldUsers;
        transactions = oldTransactions;

        cout << "\nUnable to save payment.\n";
        pauseScreen();
        return;
    }

    cout << "\nFood payment successful!\n";

    pauseScreen();
}

// ==========================================
// MOBILE LOAD
// ==========================================

void buyLoad() {
    showHeader("MOBILE LOAD");

    cout << "Enter 0 to go back.\n\n";

    string number = getInput("Mobile Number: ");

    if (number == "0") {
        return;
    }

    string provider = getInput("Network Provider: ");

    if (provider == "0") {
        return;
    }

    if (number.empty() || provider.empty()) {
        cout << "\nMobile number and provider are required.\n";
        pauseScreen();
        return;
    }

    long long amount = getAmount("Load Amount: ");

    if (!hasEnoughBalance(amount)) {
        cout << "\nInsufficient balance.\n";
        pauseScreen();
        return;
    }

    cout << "\nLOAD SUMMARY\n";
    cout << "Mobile Number: " << number << "\n";
    cout << "Provider: " << provider << "\n";
    cout << "Amount: " << formatMoney(amount) << "\n";

    if (!confirmAction()) {
        cout << "\nLoad purchase cancelled.\n";
        pauseScreen();
        return;
    }

    int index = getCurrentUserIndex();

    json oldUsers = users;
    json oldTransactions = transactions;

    users[index]["balance"] =
        users[index]["balance"].get<long long>() - amount;

    addTransaction(
        "Mobile Load",
        provider + " - " + number,
        amount
    );

    if (!saveData()) {
        users = oldUsers;
        transactions = oldTransactions;

        cout << "\nUnable to save load purchase.\n";
        pauseScreen();
        return;
    }

    cout << "\nLoad purchase successful!\n";

    pauseScreen();
}

// ==========================================
// GENERAL TRANSACTION HISTORY
// ==========================================

void showHistory(bool billsOnly) {
    showHeader(
        billsOnly
        ? "BILL PAYMENT HISTORY"
        : "TRANSACTION HISTORY"
    );

    bool found = false;

    for (size_t i = transactions.size(); i > 0; i--) {
        const json& transaction = transactions[i - 1];

        if (transaction.value("username", "") != currentUser) {
            continue;
        }

        string type = transaction.value("type", "");

        if (billsOnly &&
            type != "Bill Payment" &&
            type != "Tuition Payment") {
            continue;
        }

        found = true;

        cout << "\nDate: "
             << transaction.value("date", "") << "\n";

        cout << "Type: " << type << "\n";

        cout << "Description: "
             << transaction.value("description", "") << "\n";

        cout << "Amount: "
             << formatMoney(transaction.value("amount", 0LL)) << "\n";

        cout << "--------------------------------------------\n";
    }

    if (!found) {
        cout << "\nNo transactions found.\n";
    }

    showFooter();
    pauseScreen();
}

void transactionHistory() {
    showHistory(false);
}

// ==========================================
// DASHBOARD
// ==========================================

void dashboard() {
    while (true) {
        int index = getCurrentUserIndex();

        if (index == -1) {
            currentUser = "";
            return;
        }

        showHeader("USER DASHBOARD");

        cout << "Welcome, "
             << users[index].value("fullname", currentUser)
             << "!\n\n";

        cout << "Available Balance: "
             << formatMoney(users[index].value("balance", 0LL))
             << "\n\n";

        cout << "[1] Money Transfer\n";
        cout << "[2] Pay Bills\n";
        cout << "[3] Food Payment\n";
        cout << "[4] Mobile Load\n";
        cout << "[5] Transaction History\n";
        cout << "[6] Check Balance\n";
        cout << "[7] Logout\n";

        showFooter();

        cout << "\nSelect option: ";

        int choice = getChoice();

        switch (choice) {
            case 1:
                transferMoney();
                break;

            case 2:
                billPaymentMenu();
                break;

            case 3:
                foodPayment();
                break;

            case 4:
                buyLoad();
                break;

            case 5:
                transactionHistory();
                break;

            case 6:
                checkBalance();
                break;

            case 7:
                currentUser = "";

                cout << "\nSuccessfully logged out.\n";
                pauseScreen();

                return;

            default:
                cout << "\nInvalid option.\n";
                pauseScreen();
                break;
        }
    }
}

// ==========================================
// MAIN MENU
// ==========================================

int main() {
    loadData();

    while (true) {
        showHeader("WELCOME");

        cout << "[1] Login\n";
        cout << "[2] Register\n";
        cout << "[0] Exit\n";

        showFooter();

        cout << "\nSelect option: ";

        int choice = getChoice();

        switch (choice) {
            case 1:
                if (login()) {
                    dashboard();
                }
                break;

            case 2:
                registerAccount();
                break;

            case 0:
                showHeader("EXIT");

                cout << "Thank you for using JZC Pay!\n";
                cout << "Have a great day!\n";

                showFooter();

                return 0;

            default:
                cout << "\nInvalid option.\n";
                pauseScreen();
                break;
        }
    }

    return 0;
}
