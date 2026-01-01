import streamlit as st
from langchain_community.document_loaders import DirectoryLoader, PyPDFDirectoryLoader, TextLoader
from langchain_ollama import OllamaLLM
from langchain_text_splitters import RecursiveCharacterTextSplitter
from langchain_community.embeddings import OllamaEmbeddings
from langchain_chroma import Chroma
from langchain_core.prompts import ChatPromptTemplate
from langchain_core.runnables import RunnablePassthrough
from langchain_core.output_parsers import StrOutputParser

print("Loading documents...")
# Initialize session state for chat history
if "messages" not in st.session_state:
    st.session_state.messages = []

if "rag_chain" not in st.session_state:
    # Load documents
    loader = DirectoryLoader(
        path='./data',
        glob="./*.md",
        loader_cls=TextLoader
    )
    docs = loader.load()

    if not docs:
        st.error("No documents found in the 'data' folder. Please add some .md files.")
        st.stop()

    # Load PDF documents
    loader_pdf = PyPDFDirectoryLoader(
        path='./data',
        glob="./*.pdf",
    )
    pdf_docs = loader_pdf.load()

    loader_txt = DirectoryLoader(
        path='./data',
        glob="./*.txt",
        loader_cls=TextLoader
    )
    web_docs = loader_txt.load()

    # Split documents
    text_splitter = RecursiveCharacterTextSplitter(chunk_size=500, chunk_overlap=200)
    splits = text_splitter.split_documents(docs)
    splits_web = text_splitter.split_documents(pdf_docs)
    splits.extend(splits_web)
    splits_txt = text_splitter.split_documents(web_docs)
    print("Number of document splits:", len(splits))
    print("Creating vector store...")
    # Create vector store
    vectorstore = Chroma.from_documents(
        documents=splits,
        embedding=OllamaEmbeddings(model="mxbai-embed-large")
    )
    print("Vector store created.")

    # Create retriever and LLM
    print("Creating RAG chain...")
    retriever = vectorstore.as_retriever()
    llm = OllamaLLM(model="llama3")
    print("RAG Chain created.")
    # Define prompt template
    template = """
Answer the question based only on the following context:
{context}

Question: {question}
"""
    prompt = ChatPromptTemplate.from_template(template)


    # Build RAG chain
    def format_docs(docs):
        return "\n\n".join(doc.page_content for doc in docs)


    rag_chain = (
            {"context": retriever | format_docs, "question": RunnablePassthrough()}
            | prompt
            | llm
            | StrOutputParser()
    )

    st.session_state.rag_chain = rag_chain

# Streamlit UI
st.set_page_config(page_title="RAG Chat", layout="wide")
st.title("📚 RAG Chat Application")

# Display chat history
for message in st.session_state.messages:
    with st.chat_message(message["role"]):
        st.markdown(message["content"])

# Chat input
if prompt_input := st.chat_input("Ask a question about your documents..."):
    # Add user message to history
    st.session_state.messages.append({"role": "user", "content": prompt_input})
    with st.chat_message("user"):
        st.markdown(prompt_input)

    # Generate response
    with st.chat_message("assistant"):
        with st.spinner("Generating answer..."):
            response = st.session_state.rag_chain.invoke(prompt_input)
            st.markdown(response)

    # Add assistant message to history
    st.session_state.messages.append({"role": "assistant", "content": response})
