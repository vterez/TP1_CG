#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFMl/System.hpp>
#include <iostream>
#include <iomanip>
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
#include <sstream>
#include <string>

using namespace std::string_literals;

const int ALTURA_TELA = 600;
const int LARGURA_TELA = 800;
const int JOGADOR_BASE_Y = 550;
const int JOGADOR_BASE_X = 450;
const int DELTA_Y = 50;
const int INIMIGOS_BASE_Y = 50;
const int NUMERO_FILAS = 5;
const int LINHA_BASE = INIMIGOS_BASE_Y + NUMERO_FILAS*DELTA_Y;
const int INIMIGOS_POR_FILA = 2;
const int VELOCIDADE_PROJETIL = 4;
const int MAX_ATAQUES_POR_LEVA = 8;
const int MIN_ATAQUES_POR_LEVA = 3;
const int LIMITE_DIREITO = LARGURA_TELA-25;
const int LIMITE_ESQUERDO = 25;
const int ESPACO_INIMIGOS = (LIMITE_DIREITO-LIMITE_ESQUERDO)/INIMIGOS_POR_FILA;
const int TAMANHO_INIMIGO = 20;
const int VIDAS_INICIAIS = 10;
const int META_PONTOS = 20;
const int PONTOS_BONUS = 150;
const float TEMPO_ENTRE_ATAQUES_BASE = 0.9;
const wchar_t* MENSAGENS_GAMEOVER[] = 
{
    L"Meta de pontos atingida.\nPressione o botão direito do mouse\npara continuar.",
    L"Acabaram suas vidas.\nPressione o botão direito do mouse\npara reiniciar.",
    L"Um inimigo chegou no planeta.\nPressione o botão direito do mouse\npara reiniciar.",
};

const sf::Color COR_HEROI = sf::Color(25,107,48);
const sf::Color COR_TIRO_HEROI = sf::Color(222,223,219);
const sf::Color COR_INIMIGO = sf::Color(237,34,23);
const sf::Color COR_TIRO_INIMIGO = sf::Color(237,121,23);
const sf::Color COR_IMMUNE = sf::Color(253,235,12);
const sf::Color COR_BARREIRA = sf::Color(120,39,143);
const sf::Color COR_FORMACAO = sf::Color::Magenta;

//Nao determinismo
std::default_random_engine generator (std::chrono::system_clock::now().time_since_epoch().count());
std::uniform_real_distribution dist_posicao = std::uniform_real_distribution<double>(0,ESPACO_INIMIGOS-TAMANHO_INIMIGO);
std::uniform_int_distribution dist_num_ataques = std::uniform_int_distribution(MIN_ATAQUES_POR_LEVA,MAX_ATAQUES_POR_LEVA);
std::uniform_int_distribution dist_filas = std::uniform_int_distribution(0,NUMERO_FILAS-1);
std::uniform_int_distribution dist_atacantes = std::uniform_int_distribution(0,INIMIGOS_POR_FILA-1);
std::uniform_real_distribution dist_velocidade_formacao = std::uniform_real_distribution<float>(0.5,1.0);
std::uniform_int_distribution dist_raio = std::uniform_int_distribution(LIMITE_ESQUERDO,LIMITE_DIREITO);
std::discrete_distribution dist_formacao {35,35,25,5};
std::discrete_distribution dist_bonus {0,20,20,20,20,10,10}; //25,15,20,10,10,10,10

//Controles de jogo
std::atomic_bool gameover = false, immune = false, pause = false, jogando = true;
std::atomic<float> tempo_entre_ataques = TEMPO_ENTRE_ATAQUES_BASE;
std::atomic<int> timers_rodando = 0, numero_imunes;
std::mutex mutex_projeteis_inimigos, mutex_inimigos, mutex_jogador, mutex_bonus;
std::chrono::steady_clock::time_point tempo_inicial;
std::chrono::duration<float> tempo_jogado(std::chrono::seconds{});
bool atingiu_meta = false, super_tiro = false, existe_barreira = false, congela = false;

//Utilidades
sf::Font font=sf::Font();
sf::Text texto_pontos, texto_vidas, texto_gameover, texto_motivo_gameover, texto_nivel;
int pontos = 0;
int inimigos_disponiveis;
int velocidade_projetil = VELOCIDADE_PROJETIL;
int variacao_de_pontos = 2*velocidade_projetil*(1.5 - tempo_entre_ataques.load());
sf::RectangleShape barreira = sf::RectangleShape({LARGURA_TELA,5});

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

enum class Bonus 
{
    Normal, //237,34,23; 237,121,23
    SuperTiro, //222,223,219
    Pontos, //254,188,58
    Imunidade, //253,235,12
    VidaExtra, //25,107,48
    Congela,  //174,218,218
    Barreira //120,39,143
};

std::ostream& operator<<(std::ostream& os, const sf::Vector2f& vec)
{
    os <<"("<<vec.x<<","<<vec.y<<")";
    return os;
}

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

inline void contabiliza_tempo()
{
    auto tempo_final = std::chrono::steady_clock::now();
    tempo_jogado += (tempo_final - tempo_inicial);
}

void monta_gameover()
{
    contabiliza_tempo();
    if (motivo_gameover != MotivoGameover::Meta)
        texto_gameover.setString(L"Fim de jogo");
    else
        texto_gameover.setString(L"Você venceu");
    texto_motivo_gameover.setString(MENSAGENS_GAMEOVER[static_cast<std::underlying_type<MotivoGameover>::type>(motivo_gameover)]);
    std::wstringstream ss;
    ss << std::fixed << std::setprecision(2) <<L"Você fez "<<std::to_wstring(pontos)+L" pontos\nem "<<tempo_jogado.count()<<L" segundos.";
    texto_pontos.setString(ss.str());
    texto_pontos.setPosition(LARGURA_TELA/2-100,ALTURA_TELA/2 + 100);
}

class Projetil : public sf::Drawable
{
    public:

        sf::RectangleShape _figura;
        int _velocidade;
        sf::FloatRect _rect;
        bool _apagar;
        bool _super;

        Projetil(const sf::Vector2f& pos):_figura(sf::Vector2f(5,20)), _velocidade(velocidade_projetil), _apagar(false), _super(false)
        {
            poenomeio(_figura);
            _figura.setPosition(pos);
            _figura.setFillColor(COR_TIRO_INIMIGO);
            _rect = _figura.getGlobalBounds();
        }
        Projetil(const sf::Vector2f& pos, bool super):_figura(sf::Vector2f(super ? 50 : 5,20)), _velocidade(-velocidade_projetil), _apagar(false), _super(super)
        {
            poenomeio(_figura);
            _figura.setPosition(pos);
            _figura.setFillColor(COR_TIRO_HEROI);
            _rect = _figura.getGlobalBounds();
            super_tiro = false;
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
        static int _height, _x;
        std::atomic<bool> _existe;
        std::unique_ptr<FormacaoClass> _padrao_movimentacao;

        Inimigo(): _figura(sf::Vector2f(TAMANHO_INIMIGO,20)), _existe(true)
        {
            _figura.setPosition(dist_posicao(generator)+_x,_height);
            _figura.setFillColor(COR_INIMIGO);
            _rect = _figura.getGlobalBounds();
            _x += ESPACO_INIMIGOS;
        }
        Inimigo& operator= (Inimigo&& obj) = default;
        Inimigo (Inimigo&& obj) = default;

        inline void ataca()
        {
            if (_existe)
                projeteis_inimigos.emplace_back(_figura.getPosition());
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

        inline void move()
        {
            if (!_existe) return;
            _figura.setPosition(_padrao_movimentacao->proxima_iteracao());
            _rect = _figura.getGlobalBounds();
            if (_figura.getPosition().y > JOGADOR_BASE_Y+20)
            {
                if (existe_barreira)
                {
                    existe_barreira = false;
                    _existe = false;
                    --inimigos_disponiveis;
                    return;
                }
                motivo_gameover = MotivoGameover::InimigoChegouNoPlaneta;
                gameover = true;
                monta_gameover();
            }   
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
int Inimigo::_x = LIMITE_ESQUERDO;

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
            _figura.setFillColor(COR_HEROI);
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
using Inimigo_Bonus = std::pair<Inimigo*,Bonus>;
Inimigo_Bonus bonus = {nullptr,Bonus::Normal};

void timer(std::function<void()> f, float t)
{
    timers_rodando++;
    for (float i=0; i<t && !gameover; i+=0.5)
        sf::sleep(sf::seconds(0.5));
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

inline void remove_immunity()
{
    if (--numero_imunes == 0)
    {
        mutex_jogador.lock();
        jogador._figura.setFillColor(COR_HEROI);
        mutex_jogador.unlock();
        immune = false;
    }
}

inline void atualiza_pontos()
{
    texto_pontos.setString(std::to_string(pontos)+" pontos"s);
}

inline void atualiza_vidas()
{
    texto_vidas.setString(std::to_string(jogador._vidas)+" vidas"s);
}

inline void atualiza_dificuldade()
{
    texto_nivel.setString("Dificuldade:"s+std::to_string(variacao_de_pontos));
}

void processa_bonus(Bonus bonus)
{
    switch (bonus)
    {
        case Bonus::Pontos:
        {
            pontos += PONTOS_BONUS;
            atualiza_pontos();
            break;
        }
        case Bonus::Barreira:
        {
            existe_barreira = true;
            break;
        }
        case Bonus::Congela:
        {
            congela = true;
            std::thread(timer,[]{congela = false;},5).detach();
            break;
        }
        case Bonus::Imunidade:
        {
            immune = true;
            mutex_jogador.lock();
            jogador._figura.setFillColor(COR_IMMUNE);
            mutex_jogador.unlock();
            ++numero_imunes;
            std::thread(timer,remove_immunity,5).detach();
            break;
        }
        case Bonus::SuperTiro:
        {
            super_tiro = true;
            break;
        }
        case Bonus::VidaExtra:
        {
            ++jogador._vidas;
            atualiza_vidas();
            break;
        }
        case Bonus::Normal:
            break;
    }
}

void checa_colisao_jogador()
{
    if (!immune)
    {
        auto it = std::find_if(projeteis_inimigos.begin(), projeteis_inimigos.end(),[](auto& item)
        {
            if(jogador._rect.contains(item._figura.getPosition())) 
            {
                mutex_jogador.lock();
                jogador._figura.setFillColor(COR_IMMUNE);
                mutex_jogador.unlock();
                if (--jogador._vidas == 0)
                {
                    gameover = true;
                    motivo_gameover = MotivoGameover::SemVidas;
                    monta_gameover();
                    return true;
                }
                atualiza_vidas();
                immune = true;
                ++numero_imunes;
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
    if(std::erase_if(projeteis_jogador,[](Projetil& proj)
    {
        if (proj._apagar)
            return true;

        //checa bonus
        mutex_bonus.lock();
        auto inimigo_bonus = bonus;
        mutex_bonus.unlock();
        if (inimigo_bonus.first && 
            inimigo_bonus.first->_existe &&
            proj._rect.intersects(inimigo_bonus.first->_rect))
        {
            processa_bonus(inimigo_bonus.second);
            pontos += variacao_de_pontos;
            inimigo_bonus.first->_figura.setFillColor(sf::Color(123,184,63));
            inimigo_bonus.first->_existe = false;
            --inimigos_disponiveis;
            if (!proj._super)
                return true;
        }
        //confere outlier
        auto it = std::find_if(std::begin(fora_de_formacao), std::end(fora_de_formacao),[&proj] (Inimigo* inimigo)
        {
            if (inimigo->_existe && proj._rect.intersects(inimigo->_rect))
            {
                inimigo->_existe = false;
                --inimigos_disponiveis;
                pontos += variacao_de_pontos;
                if (!proj._super)
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
                    pontos += variacao_de_pontos;
                    if (!proj._super)
                    return true;
                }
                return false;
            });
            return it != std::end(fila_inimigos[fila]);
        }
        return false;
    }))
    {
        if (!atingiu_meta && pontos >= META_PONTOS)
        {
            gameover = true;
            motivo_gameover = MotivoGameover::Meta;
            monta_gameover();
            atingiu_meta = true;
            texto_gameover.setString(L"Você venceu");
            return;
        }
        atualiza_pontos();
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
        if (!congela)
        {
            for (auto& i:projeteis_inimigos)
                i.move();
            checa_colisao_jogador();
        }
        mutex_projeteis_inimigos.unlock();
        checa_colisao_inimigos();
        mutex_inimigos.lock();
        if (!congela)
            for (auto i:fora_de_formacao)
                i->move();
        for (auto& i: fila_inimigos)
        {
            for (auto& j: i)
                window.draw(j);
        }    
        mutex_inimigos.unlock();
        for (auto& i:projeteis_jogador)
            window.draw(i);
        for (auto& i:projeteis_inimigos)
            window.draw(i);
        mutex_jogador.lock();
        window.draw(jogador);
        mutex_jogador.unlock();
        window.draw(texto_pontos);
        window.draw(texto_vidas);
        window.draw(texto_nivel);
        if (existe_barreira)
            window.draw(barreira);
    }
    else
    {
        window.draw(texto_gameover);
        window.draw(texto_motivo_gameover);
        window.draw(texto_pontos);
    }
    window.display();
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

void thread_tira_de_formacao()
{
    int formacao;
    while (jogando)
    {
        sf::sleep(sf::seconds(1));
        if(gameover)
        {
            sf::sleep(sf::seconds(3));
            continue;
        }
        if (congela || pause || !(formacao = dist_formacao(generator)))
            continue;
        mutex_inimigos.lock();
        std::erase_if(fora_de_formacao,[](Inimigo* proj){return !proj->_existe.load();});
        switch (formacao)
        {
            case 0:
            {
                break;
            }
            case 3:
            {
                int fila = dist_filas(generator);
                for (auto it = fila_inimigos[fila].begin(); it != fila_inimigos[fila].end();++it)
                {
                    if (it->_existe)
                    {
                        it->_figura.setFillColor(sf::Color::Magenta);
                        it->nova_formacao(Formacao::Vertical,{0,0.2},0);
                        fora_de_formacao.emplace_back(it);
                    }
                }
                break;
            }
            default:
            {
                auto it = acha_inimigo();
                if (it)
                {
                    if(it->nova_formacao(Formacao{formacao-1},{dist_velocidade_formacao(generator),dist_velocidade_formacao(generator)},dist_raio(generator)))
                    {
                        it->_figura.setFillColor(sf::Color::Magenta);
                        fora_de_formacao.emplace_back(it);
                    } 
                }
                break;
            }
        }
        mutex_inimigos.unlock();
    }
}

void thread_ataque_inimigos()
{
    std::unique_lock lk (mutex_projeteis_inimigos,std::defer_lock);
    while (jogando)
    {
        sf::sleep(sf::seconds(tempo_entre_ataques));
        if (gameover)
        {
            sf::sleep(sf::seconds(3));
            continue;
        }
        if (!pause && !congela)
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

void thread_cria_bonus()
{
    int tipo_bonus;
    Inimigo* it;
    Bonus b;
    while (jogando)
    {
        sf::sleep(sf::seconds(5));
        if (gameover)
        {
            sf::sleep(sf::seconds(5));
            continue;
        }
        if (pause || congela || !(tipo_bonus = dist_bonus(generator)) || !(it = acha_inimigo()))
        {
            mutex_bonus.lock();
            bonus.first = nullptr;
            mutex_bonus.unlock();
            continue;
        }
        mutex_inimigos.lock();
        mutex_bonus.lock();
        if (bonus.first && !bonus.first->_padrao_movimentacao)
            bonus.first->_figura.setFillColor(COR_INIMIGO);
        
        switch (b=Bonus{tipo_bonus})
        {
            case Bonus::Barreira:
            {
                it->_figura.setFillColor(COR_BARREIRA);
                std::cout<<"Barreira\n";
                break;
            }
            case Bonus::Congela:
            {
                it->_figura.setFillColor(sf::Color(174,218,218));
                std::cout<<"Congela\n";
                break;
            }
            case Bonus::Imunidade:
            {
                it->_figura.setFillColor(COR_IMMUNE);
                std::cout<<"Imunidade\n";
                break;
            }
            case Bonus::Pontos:
            {
                it->_figura.setFillColor(sf::Color(0,255,64));
                std::cout<<"Pontos\n";
                break;
            }
            case Bonus::SuperTiro:
            {
                it->_figura.setFillColor(COR_TIRO_HEROI);
                std::cout<<"SuperTiro\n";
                break;
            }
            case Bonus::VidaExtra:
            {
                it->_figura.setFillColor(COR_HEROI);
                std::cout<<"VidaExtra\n";
                break;
            }
        }
        bonus = {it,b};
        mutex_bonus.unlock();
        mutex_inimigos.unlock();
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
    texto_vidas = texto_nivel = texto_pontos;
    texto_nivel.setPosition(400,0);
    barreira.setFillColor(COR_BARREIRA);
    barreira.setPosition(0,JOGADOR_BASE_Y+20);    
    
    auto t1 = std::thread(timer,thread_ataque_inimigos,3);
    auto t2 = std::thread(timer,thread_tira_de_formacao,5); 
    auto t3 = std::thread(timer,thread_cria_bonus,3);

    //Label pro goto (reset de jogo)
    jogo_zerado:

    Inimigo::_height = 50; 
    if (pontos < META_PONTOS)
        atingiu_meta = false; 
    existe_barreira = false;
    inimigos_disponiveis = NUMERO_FILAS * INIMIGOS_POR_FILA;
    velocidade_projetil = VELOCIDADE_PROJETIL;
    tempo_entre_ataques.store(TEMPO_ENTRE_ATAQUES_BASE);
    variacao_de_pontos = 2*velocidade_projetil*(1.5 - tempo_entre_ataques.load());
    projeteis_jogador.clear();
    projeteis_inimigos.clear();
    fila_inimigos.clear();
    fora_de_formacao.clear();

    for (int i=0; i<NUMERO_FILAS; i++)
    {
        Inimigo::_x = LIMITE_ESQUERDO;
        fila_inimigos.emplace_back();
        Inimigo::_height += DELTA_Y;
    }

    //controle de lapso de tempo
    float dt = 1.f/80.f; 
    float acumulador = 0.f;
    bool desenhou = false;
    sf::Clock clock;

    //label para o continuar após meta
    jogo_continuado:
    std::cout<<"Tempo:"<<tempo_jogado.count()<<"\n";
    pause = false;
    gameover = false;   
    bool pause_print=false;   
    jogador._figura.setFillColor(COR_HEROI);
    texto_pontos.setPosition(0,0);
    texto_vidas.setPosition(200,0);
    atualiza_pontos();
    atualiza_vidas();
    atualiza_dificuldade();
    tempo_inicial = std::chrono::steady_clock::now();
    while (window.isOpen())
    {
        auto pos = sf::Mouse::getPosition(window);
        int x = pos.x;
        int y = pos.y;
        if (!pause && (x < 0 || y < 0 || x > LARGURA_TELA -5|| y > ALTURA_TELA-5))
        {
            if (x < 0)
                x = 0;
            else if (x > LARGURA_TELA-5)
                x = LARGURA_TELA-5;

            if (y < 0)
                y = 0;
            else if (y > ALTURA_TELA-5)
                y = ALTURA_TELA-5;

            sf::Mouse::setPosition(sf::Vector2i(x,y), window);
        }
        //controle de lapso de tempo
        acumulador += clock.restart().asSeconds();
        while(acumulador >= dt)
        {
            if (pause)
            {
                sf::sleep(sf::milliseconds(100));
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
                            switch (event.key.code)
                            {
                                case sf::Keyboard::R:
                                {
                                    pause = true;
                                    pontos = 0;
                                    jogador._vidas = VIDAS_INICIAIS;
                                    goto jogo_zerado;
                                    break;
                                }
                                case sf::Keyboard::Escape:
                                {
                                    gameover = true;
                                    window.close();
                                    break;
                                }
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
                                case sf::Keyboard::R:
                                {
                                    pause = true;
                                    pontos = 0;
                                    jogador._vidas = VIDAS_INICIAIS;
                                    goto jogo_zerado;
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
                                    float temp = tempo_entre_ataques.load();
                                    if (temp > 0.15)
                                        temp -= 0.1;
                                    tempo_entre_ataques.store(temp);
                                    variacao_de_pontos = 2*velocidade_projetil*(1.5 - tempo_entre_ataques.load());
                                    atualiza_dificuldade();
                                    break;
                                }
                                case sf::Keyboard::F2:
                                {
                                    float temp = tempo_entre_ataques.load();
                                    if (temp < 1.3)
                                        temp += 0.1;
                                    tempo_entre_ataques.store(temp);
                                    variacao_de_pontos = 2*velocidade_projetil*(1.5 - temp);
                                    atualiza_dificuldade();
                                    break;
                                }
                                case sf::Keyboard::F4:
                                {
                                    if (velocidade_projetil > 1)
                                        --velocidade_projetil;
                                    variacao_de_pontos = 2*velocidade_projetil*(1.5 - tempo_entre_ataques.load());
                                    atualiza_dificuldade();
                                    break;
                                }
                                case sf::Keyboard::F3:
                                {
                                    if (velocidade_projetil < 10)
                                        ++velocidade_projetil;
                                    variacao_de_pontos = 2*velocidade_projetil*(1.5 - tempo_entre_ataques.load());
                                    atualiza_dificuldade();
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
                                    projeteis_jogador.emplace_back(jogador._figura.getPosition(),super_tiro);
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
                                    goto jogo_continuado;
                                }
                                else
                                {
                                    pontos = 0;
                                    tempo_jogado = std::chrono::seconds{};
                                    jogador._vidas = VIDAS_INICIAIS;
                                    goto jogo_zerado;
                                }
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
                {
                    tempo_entre_ataques.store(tempo_entre_ataques.load()-0.1);
                    variacao_de_pontos = 2*velocidade_projetil*(1.5 - tempo_entre_ataques.load());
                    atualiza_dificuldade();
                }
                jogador._vidas++;
                atualiza_vidas();
                contabiliza_tempo();
                goto jogo_zerado;
            }
        }
    }   
    jogando = false;
    t3.join();
    t2.join();
    t1.join();
    while (timers_rodando.load())
        sf::sleep(sf::seconds(1));
    std::cout<<"Finish\n";
    return 0;
}