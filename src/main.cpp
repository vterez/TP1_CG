#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFMl/System.hpp>
#include <iostream>
#include <utility>
#include <random>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <deque>
#include <utility>
#include <algorithm>
#include <memory>

using namespace std::string_literals;

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
const int INIMIGOS_POR_FILA = 5;
const int VELOCIDADE_PROJETIL = 5;
const int MAX_ATAQUES_POR_LEVA = 8;
const int MIN_ATAQUES_POR_LEVA = 3;
const int LIMITE_DIREITO = LARGURA_TELA-25;
const int LIMITE_ESQUERDO = 25;
const int VIDAS_INICIAIS = 1;
const int META_PONTOS = 20;
const wchar_t* MENSAGENS_GAMEOVER[] = 
{
    L"Meta de pontos atingida.\nPressione o botão direito do mouse\npara continuar.",
    L"Acabaram suas vidas.\nPressione o botão direito do mouse\npara reiniciar.",
    L"Um inimigo chegou no planeta.\nPressione o botão direito do mouse\npara reiniciar.",
};

//Nao determinismo
std::default_random_engine generator (std::chrono::system_clock::now().time_since_epoch().count());
std::uniform_real_distribution distx = std::uniform_real_distribution<double>(LIMITE_ESQUERDO,LIMITE_DIREITO);
std::uniform_real_distribution disty = std::uniform_real_distribution<double>(0,ALTURA_TELA);
std::uniform_int_distribution dist_num_ataques = std::uniform_int_distribution(MIN_ATAQUES_POR_LEVA,MAX_ATAQUES_POR_LEVA);
std::uniform_int_distribution dist_filas = std::uniform_int_distribution(0,NUMERO_FILAS-1);
std::uniform_int_distribution dist_atacantes = std::uniform_int_distribution(0,INIMIGOS_POR_FILA-1);
std::uniform_real_distribution dist_velocidade_formacao = std::uniform_real_distribution<float>(0.5,1.0);
std::discrete_distribution dist_formacao {50,35,15};
std::discrete_distribution dist_leva_ataques {95,5};

//Controles de jogo
std::atomic_bool gameover = false, immune = false, pause = false, jogando = true;
std::atomic<float> tempo_entre_ataques = 0.9;
std::atomic<int> timers_rodando = 0;
std::mutex mutex_projeteis_inimigos, mutex_inimigos;
std::chrono::steady_clock::time_point tempo_inicial;
std::chrono::duration<float> tempo_jogado(std::chrono::seconds{});

//Utilidades
sf::Font font=sf::Font();
sf::Text texto_pontos, texto_vidas, texto_gameover, texto_motivo_gameover;
int pontos = 0;
int inimigos_disponiveis;
int variacao_de_pontos = 10*(1.5 - tempo_entre_ataques.load());

enum class MotivoGameover
{
    Meta,
    SemVidas,
    InimigoChegouNoPlaneta
} motivo_gameover;

class FormacaoClass
{
    public:
        sf::Vector2f _posicao_inicial, _posicao_atual, _velocidade;
        FormacaoClass(const sf::Vector2f& pos, const sf::Vector2f& vel) :
             _posicao_inicial(pos), _posicao_atual(pos), _velocidade(vel){};
        virtual const sf::Vector2f& proxima_iteracao() = 0;
};

class Vertical : public FormacaoClass
{
    public:
        Vertical(const sf::Vector2f& pos, const sf::Vector2f& vel) :
             FormacaoClass(pos,vel) 
        {
            _velocidade.x = 0;
        };
        inline const sf::Vector2f& proxima_iteracao() override
        {
            return _posicao_atual += _velocidade;
        }
};

class Zigzag : public FormacaoClass
{
    public:
        float _max_x, _min_x;
        Zigzag(const sf::Vector2f& pos, const sf::Vector2f& vel, float r) :
             FormacaoClass(pos,vel)
        {
            while (r > 1 && (pos.x + r > LIMITE_DIREITO || pos.x - r < LIMITE_ESQUERDO))
            {
                r /= 2;
            }
            _max_x = pos.x + r;
            _min_x = pos.x - r;
        }

        inline const sf::Vector2f& proxima_iteracao() override
        {
            if (auto p = _posicao_atual.x + _velocidade.x; p > _max_x || p < _min_x)
                _velocidade.x *= -1;
            
            return _posicao_atual += _velocidade;
        }
};

enum class Formacao
{
    Vertical,
    ZigZag
};

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

void monta_gameover()
{
    std::cout << "Chamou a monta gameover\n";
    auto tempo_final = std::chrono::steady_clock::now();
    tempo_jogado += (tempo_final - tempo_inicial);
    texto_motivo_gameover.setString(MENSAGENS_GAMEOVER[static_cast<std::underlying_type<MotivoGameover>::type>(motivo_gameover)]);
    texto_pontos.setString(L"Sua pontuação foi "s+std::to_wstring(pontos)+L"\nPontuação atingida em "s+std::to_wstring(tempo_jogado.count())+L" segundos."s);
    poenomeio(texto_pontos);
    texto_pontos.setPosition(LARGURA_TELA/2,ALTURA_TELA/2 + 100);
}

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
        std::atomic<bool> _existe;
        bool _fora_de_formacao;
        std::unique_ptr<FormacaoClass> _padrao_movimentacao;

        Inimigo(): _figura(sf::Vector2f(20,20)), _existe(true)
        {
            _figura.setPosition(distx(generator),_height);
            _figura.setFillColor(sf::Color(123,184,63));
            _rect = _figura.getGlobalBounds();
        }
        Inimigo& operator= (Inimigo&& obj) = default;
        Inimigo (Inimigo&& obj) = default;

        void operator+ (const sf::Vector2f& dif)
        {
            _figura.move(dif);
            _rect = _figura.getGlobalBounds();
        }
        inline void ataca()
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
            if (_figura.getPosition().y > JOGADOR_BASE_Y+20)
            {
                motivo_gameover = MotivoGameover::InimigoChegouNoPlaneta;
                gameover = true;
                monta_gameover();
            }   
            if (_existe)
                target.draw(_figura);
        }

        bool operator<(const Inimigo& obj)
        {
            return _figura.getPosition().x < obj._figura.getPosition().x;
        }

        inline void move(const sf::Vector2f& vec)
        {
            _figura.setPosition(_padrao_movimentacao->proxima_iteracao());
            _rect = _figura.getGlobalBounds();
        }

        bool nova_formacao(Formacao form, const sf::Vector2f& vel, float raio)
        {
            if(_padrao_movimentacao)
                return false;
            switch(form)
            {
                case Formacao::Vertical:
                {
                    _padrao_movimentacao = std::make_unique<Vertical>(_figura.getPosition(),vel);
                    break;
                }
                case Formacao::ZigZag:
                {
                    _padrao_movimentacao = std::make_unique<Zigzag>(_figura.getPosition(),vel,raio);
                    break;
                }
            }
            return true;
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

        Jogador() : _figura(3), _vidas(VIDAS_INICIAIS)
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
            if (_nova_posicao >= LIMITE_ESQUERDO && _nova_posicao < LIMITE_DIREITO)
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
std::deque<std::array<Inimigo,INIMIGOS_POR_FILA>> fila_inimigos;
std::deque<Inimigo*> fora_de_formacao(INIMIGOS_POR_FILA);
Bonus bonus=normal;

template<class ...Args>
void timer_t(std::function<void(Args&...)> f, float t, Args& ... args)
{
    timers_rodando++;
    sf::sleep(sf::seconds(t));
    f(args...);
    timers_rodando--;
}

void timer(std::function<void()> f, float t)
{
    timers_rodando++;
    sf::sleep(sf::seconds(t));
    f();
    timers_rodando--;
}

Inimigo* acha_inimigo()
{
    int inimigo = dist_atacantes(generator);
    int fila = dist_filas(generator);
    if (!fila_inimigos[fila][inimigo]._existe)
    {
        auto it = std::find_if(fila_inimigos[fila].begin(),fila_inimigos[fila].end(),[](Inimigo& inim){return inim._existe.load();});
        if (it != fila_inimigos[fila].end())
            return it;
        else
            return nullptr;
    }
    return &fila_inimigos[fila][inimigo];
}

void ataque_inimigos()
{
    std::unique_lock lk (mutex_projeteis_inimigos,std::defer_lock);
    while (jogando)
    {
        sf::sleep(sf::seconds(tempo_entre_ataques));
        if (!pause && !gameover)
        {
            int ataques = dist_num_ataques(generator);
            lk.lock();
            for (int i=0;i<ataques;++i)
            {
                auto it = acha_inimigo();
                if (it)
                    it->ataca();
            }
            lk.unlock();
        }
    }
}

inline void remove_immunity()
{
    jogador._figura.setFillColor(sf::Color::Red);
    immune = false;
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
                    motivo_gameover = MotivoGameover::SemVidas;
                    monta_gameover();
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
    //std::cout<<"Tamanho proj_heroi:"<<projeteis_jogador.size()<<"\n";    
}

void checa_colisao_inimigos()
{
    
    if(std::erase_if(projeteis_jogador,[](Projetil& proj)
    {
        if (proj._apagar)
            return true;
        //confere outlier
        auto it = std::find_if(std::begin(fora_de_formacao), std::end(fora_de_formacao),[&proj] (Inimigo* inimigo)
        {
            if (inimigo->_existe && proj._rect.intersects(inimigo->_rect))
            {
                inimigo->_existe = false;
                --inimigos_disponiveis;
                pontos += variacao_de_pontos;
                return true;
            }
            return false;
        });
        if (it != std::end(fora_de_formacao))
            return true;

        //confere padrao
        if (int y = proj._figura.getGlobalBounds().top; y < LINHA_BASE && y > INIMIGOS_BASE_Y)
        {
            int fila = y/DELTA_Y - 1;
            auto it = std::find_if(std::begin(fila_inimigos[fila]), std::end(fila_inimigos[fila]),[&proj] (Inimigo& inimigo)
            {
                if (inimigo._existe && proj._rect.intersects(inimigo._rect))
                {
                    inimigo._existe = false;
                    --inimigos_disponiveis;
                    pontos += 10 * tempo_entre_ataques.load();
                    return true;
                }
                return false;
            });
            return it != std::end(fila_inimigos[fila]);
        }
        return false;
    }))
    {
        if (pontos >= META_PONTOS)
        {
            gameover = true;
            motivo_gameover = MotivoGameover::Meta;
            monta_gameover();
        }

        texto_pontos.setString(std::to_string(pontos)+" pontos"s);
    }


}

void draw_everything(sf::RenderWindow& window)
{
    window.clear(sf::Color(200,191,231));
    if (!gameover)
    {
        jogador.move();
        for (auto& i:projeteis_jogador)
            i.move();
        mutex_projeteis_inimigos.lock();
        for (auto& i:projeteis_inimigos)
            i.move();
        checa_colisao_jogador();
        mutex_projeteis_inimigos.unlock();
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
        for (auto i:fora_de_formacao)
            i->move(sf::Vector2f(0.2,0.2));
        window.draw(jogador);
        window.draw(texto_pontos);
        window.draw(texto_vidas);
    }
    else
    {
        window.draw(texto_gameover);
        window.draw(texto_motivo_gameover);
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
    //std::cout<<nome<<obj._figura.getPosition()<<"\n";
}

void print_everything()
{
    //std::cout<<"********************************\nElementos na tela:\n";
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

void tira_de_formacao()
{
    int formacao;
    while (jogando)
    {
        sf::sleep(sf::seconds(2));
        if (gameover || !(formacao = dist_formacao(generator)))
            continue;

        std::erase_if(fora_de_formacao,[](Inimigo* proj){return !proj->_existe.load();});
        
        auto it = acha_inimigo();
        if (it)
        {
            if(it->nova_formacao(Formacao{formacao-1},{dist_velocidade_formacao(generator),dist_velocidade_formacao(generator)},distx(generator)))
            {
                it->_figura.setFillColor(sf::Color::Magenta);
                fora_de_formacao.emplace_back(it);
            }
            
        }
        else
            std::cout<<"Falhou\n";
    }
}

int main()
{
    sf::RenderWindow window(sf::VideoMode(LARGURA_TELA,ALTURA_TELA), "My game");
    font.loadFromFile("myfont.ttf");
    texto_gameover = sf::Text("Game Over",font,40);
    poenomeio(texto_gameover);
    texto_gameover.setPosition(LARGURA_TELA/2,ALTURA_TELA/2);
    texto_pontos.setFont(font);
    texto_pontos.setFillColor(sf::Color(63,72,204));
    texto_motivo_gameover = texto_pontos;
    texto_pontos.setCharacterSize(25);
    texto_vidas = texto_pontos;
    
    texto_vidas.move(200,0);
    
    auto t1 = std::thread(ataque_inimigos);
    auto t2 = std::thread(tira_de_formacao); 

    //Label pro goto (reset de jogo)
    game:
    std::cout<<"Game\n";

    texto_pontos.setString(std::to_string(pontos) + " pontos"s);
    texto_vidas.setString(std::to_string(jogador._vidas)+" vidas"s);

    Inimigo::_height = 50;
    bool pause_print=false;    
    int contador_projeteis = 0;
    inimigos_disponiveis = NUMERO_FILAS * INIMIGOS_POR_FILA;
    projeteis_jogador.clear();
    projeteis_inimigos.clear();
    fila_inimigos.clear();
    fora_de_formacao.clear();

    for (int i=0; i<NUMERO_FILAS; i++)
    {
        fila_inimigos.emplace_back();
        Inimigo::_height += DELTA_Y;
    }

    //controle de lapso de tempo
    float dt = 1.f/80.f; 
    float acumulador = 0.f;
    bool desenhou = false;
    sf::Clock clock;

    pause = false;
    tempo_inicial = std::chrono::steady_clock::now();
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
                            gameover = true;
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
                                gameover = true;
                                window.close();
                            }
                            break;
                        }
                    }
                }
            }
            else if (!gameover)
            {
                sf::Event event;
                while (!pause && window.pollEvent(event))
                {
                    // "close requested" event: we close the window
                    switch(event.type)
                    {
                        case sf::Event::Closed:
                        {
                            gameover = true;
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
                                    pause = true;
                                    goto game;
                                    break;
                                }
                                case sf::Keyboard::Escape:
                                {
                                    gameover = true;
                                    window.close();
                                    break;
                                }
                                case sf::Keyboard::F1:
                                {
                                    desloca_formacao(0);
                                    //std::cout<<"Moveu\n";
                                    break;
                                }
                                case sf::Keyboard::F2:
                                {
                                    float temp = tempo_entre_ataques.load();
                                    if (temp > 0.15)
                                        temp -= 0.1;
                                    tempo_entre_ataques.store(temp);
                                    break;
                                }
                                case sf::Keyboard::F3:
                                {
                                    float temp = tempo_entre_ataques.load();
                                    if (temp < 1.2)
                                        temp += 0.1;
                                    tempo_entre_ataques.store(temp);
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
            else
            {
                sf::Event event;
                while (window.pollEvent(event))
                {
                    switch (event.type)
                    {
                        case sf::Event::Closed:
                        {
                            gameover = true;
                            window.close();
                            break;
                        }
                        case sf::Event::KeyPressed:
                        {
                            if (event.key.code == sf::Keyboard::Escape)
                            {
                                gameover = true;
                                window.close();
                            }
                            break;
                        }
                        case sf::Event::MouseButtonPressed:
                        {
                            if (event.mouseButton.button == sf::Mouse::Right)
                            {
                                if (motivo_gameover == MotivoGameover::Meta)
                                {
                                    jogador._vidas += 1;
                                }
                                else
                                {
                                    pontos = 0;
                                    tempo_jogado = std::chrono::seconds{};
                                    jogador._vidas = VIDAS_INICIAIS;
                                }
                                gameover = false;
                                goto game;
                            }
                            break;
                        }
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
            if (inimigos_disponiveis == 0)
            {
                if (tempo_entre_ataques.load() > 0.15)
                    tempo_entre_ataques.store(tempo_entre_ataques.load()-0.1);
                goto game;
            }
        }
    }   
    t2.join();
    t1.join();
    while (timers_rodando.load())
        sf::sleep(sf::seconds(1));
    std::cout<<"Finish\n";
    return 0;
}