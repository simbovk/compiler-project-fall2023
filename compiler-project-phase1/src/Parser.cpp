#include "Parser.h"

// main point is that the whole input has been consumed
AST *Parser::parse()
{
    AST *Res = parseGoal();
    return Res;
}

AST *Parser::parseGoal()
{
    llvm::SmallVector<Expr *> exprs;
    while (!Tok.is(Token::eoi))
    {
        switch (Tok.getKind())
        {
        case Token::KW_int:
            Expr *d;
            d = parseDec();
            if (d)
                exprs.push_back(d);
            else
                goto _error2;
            break;
        case Token::ident:
            Expr *a;
            a = parseAssign();

            if (!Tok.is(Token::semicolon))
            {
                error();
                goto _error2;
            }
            if (a)
                exprs.push_back(a);
            else
                goto _error2;
            break;

        case Token::KW_if:
            Expr *e;
            e = parseCondition();

            if (e)
                exprs.push_back(e);
            else
                goto _error2;
            break;

        case Token::loop:
            Expr *l;
            l = parseLoop();

            if (l)
                exprs.push_back(l);
            else
                goto _error2;
            break;
        default:
            goto _error2;
            break;
        }
        advance(); // TODO: watch this part
    }
    return new Goal(exprs);
_error2:
    while (Tok.getKind() != Token::eoi)
        advance();
    return nullptr;
}

Expr *Parser::parseDec()
{
    Expr *E;
    llvm::SmallVector<llvm::StringRef, 8> Vars;
    llvm::SmallVector<Expr *> Numbers;
    int countIdentifiers = 0, countExprs = 0;
    if (expect(Token::KW_int))
        goto _error;

    advance();

    if (expect(Token::ident))
        goto _error;
    Vars.push_back(Tok.getText());
    countIdentifiers = 1;
    advance();

    while (Tok.is(Token::comma))
    {
        advance();
        if (expect(Token::ident))
            goto _error;
        Vars.push_back(Tok.getText());
        countIdentifiers++;
        advance();
    }

    if (Tok.is(Token::equal))
    {
        advance();
        E = parseExpr();
        if (E)
        {
            Numbers.push_back(E);
            countExprs++;
        }
        else
            goto _error;

        while (countExprs <= countIdentifiers && Tok.is(Token::comma))
        {
            advance();
            E = parseExpr();
            if (E)
            {
                Numbers.push_back(E);
                countExprs++;
                Factor *temp = (Factor *)E;
            }
            else
                goto _error;
        }
        if(countExprs > countIdentifiers){
                goto _error;
        }
    }

    if (expect(Token::semicolon))
        goto _error;

    return new Declaration(Vars, Numbers);
_error: // TODO: Check this later in case of error :)
    while (Tok.getKind() != Token::eoi)
        advance();
    return nullptr;
}

Expr *Parser::parseCondition()
{
    Expr *E;
    BE *B;

    llvm::SmallVector<Expr *> exprs;
    llvm::SmallVector<Expr *> bes;

    if (expect(Token::KW_if))
        goto _error3;

    advance();
    E = parseExpr();

    if (E)
    {
        exprs.push_back(E);
    }
    else
        goto _error3;

    if (expect(Token::colon))
        goto _error3;

    advance();
    B = (BE *)(parseBE());

    if (B)
    {
        bes.push_back(B);
    }
    else
        goto _error3;

    while (Tok.is(Token::elif))
    {
        advance();
        E = parseExpr();
        if (E)
        {
            exprs.push_back(E);
        }
        else
            goto _error3;

        if (expect(Token::colon))
            goto _error3;

        advance();
        B = (BE *)(parseBE());
        if (B)
        {
            bes.push_back(B);
        }
        else
            goto _error3;
    }

    if (Tok.is(Token::KW_else))
    {
        advance();
        if (expect(Token::colon))
            goto _error3; // error() or error3 idk :}
        advance();
        B = (BE *)parseBE();
        if (B)
        {
            bes.push_back(B);
        }
        else goto _error3;
    }
    return new Condition(exprs, bes);

_error3: // TODO: Check this later in case of error :)
    while (Tok.getKind() != Token::eoi)
        advance();
    return nullptr;
}

Expr *Parser::parseLoop()
{
    Expr *E;
    BE *B;

    if (expect(Token::loop))
        error(); // Error error() idk
        return nullptr;

    advance();
    E = parseExpr();

    if (expect(Token::colon))
        error(); // Error error() idk
        return nullptr;
    advance();

    B = (BE *)(parseBE());

    return new Loop(E, B);
}

Expr *Parser::parseAssign()
{
    Expr *E;
    Factor *F;
    F = (Factor *)(parseFactor()); // why factor not identify

    if (!Tok.is(Token::equal) && !Tok.is(Token::plus_equal) && !Tok.is(Token::minus_equal) &&
        !Tok.is(Token::remain_equal) && !Tok.is(Token::mult_equal) && !Tok.is(Token::div_equal))
    {
        error();
        return nullptr;
    }

    advance();
    E = parseExpr();
    return new Assignment(F, E);
}

Expr *Parser::parseExpr()
{
    Expr *Left = parseExpr1();
    while (Tok.is(Token::KW_or))
    {
        BinaryOp::Operator Op =
            BinaryOp::Or;
        advance();
        Expr *Right = parseExpr1();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseExpr1()
{
    Expr *Left = parseExpr2();
    while (Tok.is(Token::KW_and))
    {
        BinaryOp::Operator Op =
            BinaryOp::And;
        advance();
        Expr *Right = parseExpr2();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseExpr2()
{
    Expr *Left = parseExpr3();
    while (Tok.isOneOf(Token::equal_equal, Token::not_equal))
    {
        BinaryOp::Operator Op =
            Tok.is(Token::equal_equal) ? BinaryOp::Equal_equal : BinaryOp::Not_equal;
        advance();
        Expr *Right = parseExpr3();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseExpr3()
{
    Expr *Left = parseExpr4();
    while (Tok.isOneOf(Token::more_equal, Token::less_equal))
    {
        BinaryOp::Operator Op =
            Tok.is(Token::more_equal) ? BinaryOp::More_equal : BinaryOp::Less_equal;
        advance();
        Expr *Right = parseExpr4();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseExpr4()
{
    Expr *Left = parseExpr5();
    while (Tok.isOneOf(Token::more, Token::less))
    {
        BinaryOp::Operator Op =
            Tok.is(Token::more) ? BinaryOp::More : BinaryOp::Less;
        advance();
        Expr *Right = parseExpr5();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseExpr5()
{
    Expr *Left = parseExpr6();
    while (Tok.isOneOf(Token::plus, Token::minus))
    {
        BinaryOp::Operator Op =
            Tok.is(Token::plus) ? BinaryOp::Plus : BinaryOp::Minus;
        advance();
        Expr *Right = parseExpr6();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseExpr6()
{
    Expr *Left = parseTerm();
    while (Tok.isOneOf(Token::star, Token::slash, Token::remain))
    {
        BinaryOp::Operator Op;
        if(Tok.is(Token::star))
            Op = BinaryOp::Mul;
        else if(Tok.is(Token::slash))
            Op = BinaryOp::Div;
        else if(Tok.is(Token::remain))
            Op = BinaryOp::Remain; // new 

        advance();
        Expr *Right = parseTerm();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseTerm()
{
    Expr *Left = parseFactor();
    while (Tok.is(Token::power))
    {
        BinaryOp::Operator Op = BinaryOp::Power;
        advance();
        Expr *Right = parseFactor();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseFactor()
{
    Expr *Res = nullptr;
    switch (Tok.getKind())
    {
    case Token::number:
        Res = new Factor(Factor::Number, Tok.getText());
        advance();
        break;
    case Token::ident:
        Res = new Factor(Factor::Ident, Tok.getText());
        advance();
        break;
    case Token::l_paren:
        advance();
        Res = parseExpr();
        if (!consume(Token::r_paren))
            break;
    default: // error handling
        if (!Res)
            error();
        while (!Tok.isOneOf(Token::r_paren, Token::star, Token::plus, Token::minus, Token::slash, Token::eoi))
            advance();
        break;
    }
    return Res;
}

Expr *Parser::parseBE()
{

    Expr *E;
    llvm::SmallVector<Expr *> assigns;
    if (expect(Token::begin))
    {
        error();
        return nullptr; // idk error or error()
    }

    advance();

    while (Tok.is(Token::ident))
    {
        E = parseAssign();
        if (E)
        {
            assigns.push_back(E);
        }
        else
        {
            error();
            return nullptr;
        }
    }

    if (expect(Token::end))
    {
        error();
        return nullptr; // idk error or error()
    }

    advance();
    return new BE(assigns);
}