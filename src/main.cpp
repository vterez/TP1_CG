#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFMl/System.hpp>
#include <iostream>
#include <utility>
#include <random>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <valarray>
#include <deque>
#include <utility>
#include <parallel/algorithm>

enum Bonus {normal, points, immunity, projectile, shrink};
std::discrete_distribution dist_bonus {65,15,8,8,4};
float velocity = 3.5f;
const int HEIGHT = 600;
const int WIDTH = 800;
const int PLAYER_BASE_Y = 550;
const int PLAYER_BASE_X = 450;

std::default_random_engine generator (std::chrono::system_clock::now().time_since_epoch().count());
std::uniform_real_distribution distx = std::uniform_real_distribution<double>(0,WIDTH);
std::uniform_real_distribution disty = std::uniform_real_distribution<double>(0,HEIGHT);
std::atomic<int> current_immune(0), current_bonus(0);
std::atomic<bool> immune = false;

template <typename T>
void poenomeio(T& t)
{
    sf::Vector2f posi;
    sf::FloatRect rect;
    rect=t.getGlobalBounds();
    posi.x=rect.left+(rect.width)/2;
    posi.y=rect.top+(rect.height)/2;
    t.setOrigin(posi);
};

class Inimigo : public sf::Drawable
{
    public:

        sf::FloatRect _rect;
        sf::RectangleShape _figura;
        static int _height;
        bool _existe;

        Inimigo(): _figura(sf::Vector2f(20,20)), _existe(true)
        {
            poenomeio(_figura);
            _figura.setPosition(distx(generator),_height);
            _figura.setFillColor(sf::Color(123,184,63));
            _rect = _figura.getGlobalBounds();
        }
        Inimigo& operator= (const Inimigo& obs) = default;
        Inimigo& operator+= (const sf::Vector2f& dif)
        {
            _figura.move(dif);
            return *this;
        }
        void draw(sf::RenderTarget &target, sf::RenderStates &states) const
        {
            if (_existe)
                target.draw(_figura);
        }
        void draw(sf::RenderTarget& target, sf::RenderStates states) const
        {
            if (_existe)
                target.draw(_figura);
        }

        bool operator<(const Inimigo& obj)
        {
            return _figura.getPosition().x < obj._figura.getPosition().x;
        }
};
int Inimigo::_height = 50;

class Projetil : public sf::Drawable
{
    public:

        sf::RectangleShape _figura;
        int _velocidade;

        Projetil(int v, const sf::Vector2f& pos):_figura(sf::Vector2f(5,20)), _velocidade(v)
        {
            poenomeio(_figura);
            _figura.setPosition(pos);
            _figura.setFillColor(sf::Color::White);
            _figura.setOutlineColor(sf::Color::Black);
        }

        Projetil(Projetil&& outro) = default;
        Projetil& operator=(Projetil&& other) = default;

        bool move(sf:: RenderWindow& window)
        {
            _figura.move(0,_velocidade);
            if (int pos = _figura.getPosition().y; pos < 0 || pos > HEIGHT)
                return true;
            window.draw(_figura);
            return false;
        }

        void draw(sf::RenderTarget &target, sf::RenderStates &states) const
        {
            target.draw(_figura);
        }
        void draw(sf::RenderTarget& target, sf::RenderStates states) const
        {
            target.draw(_figura);
        }

};

class Jogador : public sf::Drawable
{
    public:
        
        sf::ConvexShape _figura;
        int _vidas;
        sf::FloatRect _rect;
        int _nova_posicao;

        Jogador() : _figura(3), _vidas(3)
        {
            _figura.setPoint(0, sf::Vector2f(55.f,60.f));
            _figura.setPoint(1, sf::Vector2f(95.f,60.f));
            _figura.setPoint(2, sf::Vector2f(75.f,15.f));
            _figura.setFillColor(sf::Color(255,0,0));
            poenomeio(_figura);
            _figura.setPosition(PLAYER_BASE_X,PLAYER_BASE_Y);
            _rect = _figura.getGlobalBounds();
        }

        inline void move()
        {
            if (_nova_posicao >= 0 && _nova_posicao < WIDTH)
            {
                _figura.setPosition(_nova_posicao,PLAYER_BASE_Y);
                _rect = _figura.getGlobalBounds();
                _nova_posicao = -1;
            }
        }

        void draw(sf::RenderTarget &target, sf::RenderStates &states) const
        {
            target.draw(_figura);
        }

        void draw(sf::RenderTarget& target, sf::RenderStates states) const
        {
            target.draw(_figura);
        }

};

Jogador jogador;
sf::Font font=sf::Font();
sf::Text text_game_over = sf::Text("Game Over",font,40);
std::vector<std::valarray<Inimigo>> fila_inimigos(5);
std::deque<Projetil> projeteis_jogador, projeteis_inimigos;
Bonus bonus=normal;
bool gameover=false;

void checa_colisao()
{
    __gnu_parallel::for_each(projeteis_inimigos.begin(), projeteis_inimigos.end(),[](auto& item){if(jogador._rect.contains(item._figura.getPosition())) std::cout<<"Jogador foi atingido";});
}

template<class ...Args>
void timer(std::atomic<int>& var, std::function<void(Args&...)> f, Args& ... args)
{
    sf::sleep(sf::seconds(5));
    if (var-- == 1)
    {
        f(args...);
    }
}

inline void color_change(sf::CircleShape& biscuit, Bonus& b)
{
    biscuit.setFillColor(sf::Color::Black);
    b = normal;
}

inline void remove_immunity(sf::RectangleShape& head)
{
    immune = false;
    head.setFillColor(sf::Color::Magenta);
    std::cout<<"Saiu do imune\n";

}

void draw_everything(sf::RenderWindow& window)
{
    window.clear(sf::Color(200,191,231));
    jogador.move();
    std::erase_if(projeteis_jogador,[&window](auto&& item) 
    {
        return item.move(window);
    });
    std::erase_if(projeteis_inimigos,[&window](auto&& item) 
    {
        return item.move(window);
    });
    checa_colisao();
    window.draw(jogador);
    for (auto& i: fila_inimigos)
    {
        for (auto& j: i)
            window.draw(j);
    }    
    if (!gameover)
    {
        
        
    
        //std::cout<<rect.top<<" "<<rect.left<<" "<<rect.height<<" "<<rect.width<<"\n";
    }
    else
    {
        window.draw(text_game_over);
    }
    window.display();
}

std::ostream& operator<<(std::ostream& os, const sf::Vector2f& vec)
{
    os <<"("<<vec.x<<","<<vec.y<<")";
    return os;
}

template <typename T>
inline void print(const char* nome, T& obj)
{
    std::cout<<nome<<obj._figura.getPosition()<<"\n";
}

void print_everything()
{
    std::cout<<"********************************\nElementos na tela:\n";
    print("Jogador",jogador);
    for (auto& i: fila_inimigos)
        for (auto& j: i)
            print("Inimigo",j);
    for (auto& i: projeteis_jogador)
        print("Projetil jogador:",i);
    for (auto& i: projeteis_inimigos)
        print("Projetil inimigo:",i);
}

int main()
{
    sf::RenderWindow window(sf::VideoMode(WIDTH,HEIGHT), "My game");
    font.loadFromFile("myfont.ttf");

    //Label pro goto (reset de jogo)
    game:
    Inimigo::_height = 50;
    bool pause=false;
    bool pause_print=false;    
    int contador_projeteis = 0;
    projeteis_jogador.clear();
    projeteis_inimigos.clear();

    for (auto& i: fila_inimigos)
    {
        i = std::valarray<Inimigo>(10);
        std::sort(std::begin(i),std::end(i));
        Inimigo::_height += 50;
    }

    //controle de lapso de tempo
    float dt = 1.f/80.f; 
    float acumulador = 0.f;
    bool desenhou = false;
    sf::Clock clock;

    while (window.isOpen())
    {
        //controle de lapso de tempo
        acumulador += clock.restart().asSeconds();
        while(acumulador >= dt)
        {
            if (pause)
            {
                sf::sleep(sf::milliseconds(50));
                sf::Event event;
                while (pause && window.pollEvent(event))
                {
                    switch(event.type)
                    {
                        case sf::Event::Closed:
                        {
                            window.close();
                            break;
                        }
                        case sf::Event::MouseButtonPressed:
                        {
                            if (event.mouseButton.button == sf::Mouse::Right)
                            {
                                pause = false;
                                pause_print = false;
                                jogador._nova_posicao = event.mouseButton.x;
                            }
                            else if (pause_print && event.mouseButton.button == sf::Mouse::Middle)
                            {
                                draw_everything(window);
                                print_everything();
                            }
                            break;
                        }
                        case sf::Event::KeyPressed:
                        {
                            if (event.key.code == sf::Keyboard::Escape)
                            {
                                window.close();
                            }
                            break;
                        }
                    }
                }
            }
            else
            {
                sf::Event event;
                while (window.pollEvent(event))
                {
                    // "close requested" event: we close the window
                    switch(event.type)
                    {
                        case sf::Event::Closed:
                        {
                            window.close();
                            break;
                        }
                        case sf::Event::KeyPressed:
                        {
                            switch(event.key.code)
                            {
                                case sf::Keyboard::Enter:
                                {
                                    projeteis_inimigos.emplace_back(5,sf::Vector2f(400,100));
                                    break;
                                }
                                
                                case sf::Keyboard::R:
                                {
                                    goto game;
                                    break;
                                }
                                case sf::Keyboard::Escape:
                                {
                                    window.close();
                                    break;
                                }
                                default:
                                    break;
                            }
                            break;
                        }
                        case sf::Event::MouseMoved:
                        {
                            jogador._nova_posicao = event.mouseMove.x;
                            break;
                        }
                        case sf::Event::MouseButtonPressed:
                        {
                            switch(event.mouseButton.button)
                            {
                                case sf::Mouse::Left:
                                {
                                    projeteis_jogador.emplace_back(-5,jogador._figura.getPosition());
                                    break;
                                }
                                case sf::Mouse::Right:
                                {
                                    pause = true;
                                    break;
                                }
                                case sf::Mouse::Middle:
                                {
                                    pause = true;
                                    pause_print = true;
                                    print_everything();
                                    break;
                                }
                                default:
                                {

                                }
                            }                        
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
            
            acumulador-=dt;
            desenhou=false;
        }
        if(desenhou || pause)
            sf::sleep(sf::seconds(0.01f)); 
        else
        {
            desenhou=1;
            draw_everything(window);
        }
    }   

    std::cout<<"Finish\n";
    return 0;
}