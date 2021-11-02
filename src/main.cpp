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
#include <deque>
#include <utility>
#include <algorithm>

enum Bonus {normal, points, immunity, projectile, shrink};
std::discrete_distribution dist_bonus {65,15,8,8,4};
const int ALTURA_TELA = 600;
const int LARGURA_TELA = 800;
const int JOGADOR_BASE_Y = 550;
const int JOGADOR_BASE_X = 450;
const int DELTA_Y = 50;
const int INIMIGOS_BASE_Y = 50;
const int NUMERO_FILAS = 5;
const int LINHA_BASE = INIMIGOS_BASE_Y + NUMERO_FILAS*DELTA_Y;
const int INIMIGOS_POR_FILA = 10;
const int VELOCIDADE_PROJETIL = 5;
const int MAX_ATAQUES_POR_LEVA = 9;
const int MIN_ATAQUES_POR_LEVA = 4;

std::default_random_engine generator (std::chrono::system_clock::now().time_since_epoch().count());
std::uniform_real_distribution distx = std::uniform_real_distribution<double>(50,LARGURA_TELA-50);
std::uniform_real_distribution disty = std::uniform_real_distribution<double>(0,ALTURA_TELA);
std::uniform_int_distribution dist_num_ataques = std::uniform_int_distribution(MIN_ATAQUES_POR_LEVA,MAX_ATAQUES_POR_LEVA);
std::uniform_int_distribution dist_filas = std::uniform_int_distribution(0,NUMERO_FILAS-1);
std::uniform_int_distribution dist_atacantes = std::uniform_int_distribution(0,INIMIGOS_POR_FILA-1);
std::atomic_bool gameover = false, immune = false;

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

class Projetil : public sf::Drawable
{
    public:

        sf::RectangleShape _figura;
        int _velocidade;
        sf::FloatRect _rect;
        bool _apagar;

        Projetil(int v, const sf::Vector2f& pos):_figura(sf::Vector2f(5,20)), _velocidade(v), _apagar(false)
        {
            poenomeio(_figura);
            _figura.setPosition(pos);
            _figura.setFillColor(sf::Color::White);
            _figura.setOutlineColor(sf::Color::Black);
            _rect = _figura.getGlobalBounds();
        }

        Projetil(Projetil&& outro) = default;
        Projetil& operator=(Projetil&& other) = default;

        void move()
        {
            _figura.move(0,_velocidade);
            _rect.top += _velocidade;
            if (int pos = _figura.getPosition().y; pos < 0 || pos > ALTURA_TELA)
            {
                _apagar = true;
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

std::deque<Projetil> projeteis_jogador, projeteis_inimigos;

class Inimigo : public sf::Drawable
{
    public:

        sf::FloatRect _rect;
        sf::RectangleShape _figura;
        static int _height;
        std::atomic_bool _existe;

        Inimigo(): _figura(sf::Vector2f(20,20)), _existe(true)
        {
            _figura.setPosition(distx(generator),_height);
            _figura.setFillColor(sf::Color(123,184,63));
            _rect = _figura.getGlobalBounds();
        }
        Inimigo& operator= (const Inimigo& obs) = default;
        void operator+ (const sf::Vector2f& dif)
        {
            _figura.move(dif);
            _rect = _figura.getGlobalBounds();
        }
        void ataca()
        {
            if (_existe)
                projeteis_inimigos.emplace_back(VELOCIDADE_PROJETIL,_figura.getPosition());
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
int Inimigo::_height = INIMIGOS_BASE_Y;

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
            _figura.setPosition(JOGADOR_BASE_X,JOGADOR_BASE_Y);
            _rect = _figura.getGlobalBounds();
        }

        inline void move()
        {
            if (_nova_posicao >= 0 && _nova_posicao < LARGURA_TELA)
            {
                _figura.setPosition(_nova_posicao,JOGADOR_BASE_Y);
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
std::vector<std::array<Inimigo,INIMIGOS_POR_FILA>> fila_inimigos;
std::deque<Inimigo> fora_de_formacao(INIMIGOS_POR_FILA);
Bonus bonus=normal;


void perdeu_vida()
{

}

template<class ...Args>
void timer_t(std::function<void(Args&...)> f, Args& ... args)
{
    sf::sleep(sf::seconds(3));
    f(args...);
}

void timer(std::function<void()> f, float t)
{
    sf::sleep(sf::seconds(t));
    f();
}

void ataque_inimigos()
{
    int ataques = dist_num_ataques(generator);
    int inimigo, fila;
    for (int i=0;i<ataques;++i)
    {
        inimigo = dist_atacantes(generator);
        fila = dist_filas(generator);
        fila_inimigos[fila][inimigo].ataca();
    }
}

inline void color_change(sf::CircleShape& biscuit, Bonus& b)
{
    biscuit.setFillColor(sf::Color::Black);
    b = normal;
}

inline void remove_immunity()
{
    jogador._figura.setFillColor(sf::Color::Red);
    immune = false;
    std::cout<<"Saiu do imune\n";
}


void checa_colisao_jogador()
{
    if (!immune)
    {
        auto it = std::find_if(projeteis_inimigos.begin(), projeteis_inimigos.end(),[](auto& item)
        {
            if(jogador._rect.contains(item._figura.getPosition())) 
            {
                jogador._figura.setFillColor(sf::Color::Black);
                if (--jogador._vidas == 0)
                {
                    gameover = true;
                    return true;
                }
                immune = true;
                std::thread(timer,remove_immunity,3).detach();
                item._apagar = true;
                return true;
            };
            return false;
        });
        if (!gameover && it != projeteis_inimigos.end())
        {
            projeteis_inimigos.clear();
        }
        else
            std::erase_if(projeteis_inimigos,[](Projetil& proj){return proj._apagar;});
    }    
}

void checa_colisao_inimigos()
{
    std::erase_if(projeteis_jogador,[](Projetil& proj)
    {
        if (proj._apagar)
            return true;
        //confere outlier

        //confere padrao
        if (int y = proj._figura.getGlobalBounds().top; y < LINHA_BASE && y > INIMIGOS_BASE_Y)
        {
            int fila = y/DELTA_Y - 1;
            auto it = std::find_if(std::begin(fila_inimigos[fila]), std::end(fila_inimigos[fila]),[&proj] (Inimigo& inimigo)
            {
                if (inimigo._existe && proj._rect.intersects(inimigo._rect))
                {
                    std::cout<<"Foi\n";
                    inimigo._existe = false;
                    return true;
                }
                return false;
            });
            return it != std::end(fila_inimigos[fila]);
        }
        return false;
    });

}

void draw_everything(sf::RenderWindow& window)
{
    window.clear(sf::Color(200,191,231));
    jogador.move();
    for (auto& i:projeteis_jogador)
        i.move();
    for (auto& i:projeteis_inimigos)
        i.move();
    checa_colisao_jogador();
    checa_colisao_inimigos();
    for (auto& i: fila_inimigos)
    {
        for (auto& j: i)
            window.draw(j);
    }    
    for (auto& i:projeteis_jogador)
        window.draw(i);
    for (auto& i:projeteis_inimigos)
        window.draw(i);
    window.draw(jogador);
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

void desloca_formacao(int n)
{
    std::for_each(fila_inimigos[n].begin(),fila_inimigos[n].end(),[](Inimigo& item){return item + sf::Vector2f(5,10);});
}

int main()
{
    sf::RenderWindow window(sf::VideoMode(LARGURA_TELA,ALTURA_TELA), "My game");
    font.loadFromFile("myfont.ttf");

    //Label pro goto (reset de jogo)
    game:
    Inimigo::_height = 50;
    bool pause=false;
    bool pause_print=false;    
    int contador_projeteis = 0;
    projeteis_jogador.clear();
    projeteis_inimigos.clear();
    fila_inimigos.clear();
    fora_de_formacao.clear();

    fila_inimigos.reserve(NUMERO_FILAS);
    for (int i=0; i<NUMERO_FILAS; i++)
    {
        fila_inimigos.emplace_back();
        Inimigo::_height += DELTA_Y;
    }

    std::thread(ataque_inimigos).detach();
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
                                    //projeteis_inimigos.emplace_back(5,sf::Vector2f(400,100));
                                    ataque_inimigos();
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
                                case sf::Keyboard::F1:
                                {
                                    desloca_formacao(0);
                                    std::cout<<"Moveu\n";
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