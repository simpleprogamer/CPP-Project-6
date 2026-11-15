
#include <iostream>
#include <exception>
#include <coroutine>
#include <utility>
#include <optional>
#include <string>
#include <charconv>



namespace
{
    constexpr int kMinNumber = 1;
    constexpr int kMaxNumber = 100;
}
enum class Outcome{Inprogress, Guessed, Contradiction};
 
class Guesser
{
public:
    struct promise_type
{
    int guess = 0;
    int reaction = 0;
    int attempts = 0;
    Outcome outcome = Outcome::Inprogress;
    std::exception_ptr error;
    Guesser get_return_object()
    {
        return Guesser( std::coroutine_handle<promise_type>::from_promise(*this));
    }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_value(Outcome result) noexcept { outcome = result; }
    void unhandled_exception() { error = std::current_exception(); }
};
    using Handle = std::coroutine_handle<promise_type>;
    struct Proposal
    {
        int value;
        Handle handle{};
        bool await_ready() const noexcept {
            return false;
        }
        void await_suspend(Handle h) noexcept
        {
            handle = h;
            h.promise().guess = value;
            ++h.promise().attempts;
        }
        int await_resume() const noexcept { return handle.promise().reaction; }
    };
    Guesser(const Guesser&) = delete;
    Guesser& operator=(const Guesser&) = delete;
    Guesser(Guesser&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Guesser& operator=(Guesser&& other) noexcept 
    {
        if (this != &other)
        {
            destroy();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
        }
    ~Guesser() {
        destroy();
    }
    void start() {
        resumeandcheck();
    }
    void respond(int reaction)
    {
        handle_.promise().reaction = reaction;
        resumeandcheck();
    }
    bool done() const { return handle_.done(); }
    int currentguess() const { return handle_.promise().guess; }
    int attempts() const { return handle_.promise().attempts; }
    Outcome outcome() const { return handle_.promise().outcome; }
private:
    explicit Guesser(Handle handle): handle_(handle){}
    void resumeandcheck()
    {
        handle_.resume();
        if (handle_.promise().error) std::rethrow_exception(handle_.promise().error);
    }
    void destroy()
    {
        if(handle_)
        {
            handle_.destroy();
            handle_ = {};
        }
    }
    Handle handle_;
};
Guesser guessNumber(int low, int high)
{
    while(low <= high)
    {
        const int guess = low + (high - low) / 2;
        const int reaction = co_await Guesser::Proposal{ guess };
        if (reaction == 0) co_return Outcome::Guessed;
        if (reaction < 0) low = guess + 1; else high = guess - 1;
    }
    co_return Outcome::Contradiction;
}
std::string_view trim(std::string_view text)
{
    constexpr std::string_view kSpaces = "\t\r\n";
    const auto first = text.find_first_not_of(kSpaces);
    if (first == std::string_view::npos) return {};
    const auto last = text.find_last_not_of(kSpaces);
    return text.substr(first, last - first + 1);
}

std::optional<int> parsereaction(std::string_view text)
{
    text = trim(text);
    int value = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size()) return std::nullopt;
    if (value < -1 || value > 1) return std::nullopt;
    return value;

}

std::optional<int> readReaction(std::istream& in, std::ostream& out)
{
    std::string line;
    while(true)
    {
        out << "Your reaction (-1, 0 or 1)";
        if (!std::getline(in, line)) return std::nullopt;
        if (const auto reaction = parsereaction(line)) return reaction;
        out << "Error: choose basically -1, 0 or 1.\n";
    }
}
void printResult(const Guesser& guesser)
{
    std::cout << "Results";
    switch(guesser.outcome())
    {
    case Outcome::Guessed:
        std::cout << "Guessed number:" << guesser.currentguess() << ". Guessed in " << guesser.attempts() << " guess(es).\n";
        break;
    case Outcome::Contradiction:
        std::cout << "Your guesses are contradictory: numbers that satisfy all reactions, dont exist.\n";
        break;
    case Outcome::Inprogress:
        std::cout << "Game over";
        break;
    }
}
void playGame()
{
    std::cout << "Guess the number from " << kMinNumber << " to " << kMaxNumber << ".\n" << " Reaction: -1 - the number program gave is lesser than currently guessed one, 0 - equal, 1 - greater.\n";
    Guesser guesser = guessNumber(kMinNumber, kMaxNumber);
    guesser.start();

    while (!guesser.done())
    {
        std::cout << "Guess attempt" << guesser.attempts() << ": is the number" << guesser.currentguess() << "?\n";
        const auto reaction = readReaction(std::cin, std::cout);
        if (!reaction) break;
        guesser.respond(*reaction);
    }
    printResult(guesser);
}
int main()
{

    try
    {
        playGame();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error:" << e.what() << '\n';
        return 1;
    }
    return 0;
}
